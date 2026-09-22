// Shiny Engine: original spatial SDR preview. No vendor DLL loading or neural weights.
#include <windows.h>
#include <shobjidl.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <bcrypt.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
constexpr UINT REQUEST = WM_APP + 1, FRAME = WM_APP + 2, SOURCE_CLOSED = WM_APP + 3;
constexpr UINT PICK_BUTTON = 101, PAUSE_BUTTON = 102, STOP_BUTTON = 103, COMPARE_BUTTON = 104;
struct Options { float strength = .4f, denoise = .08f, split = .5f; bool enabled = true; };
struct Request { std::string id, command, session; Options options; };
bool token(std::string const& value) {
  return !value.empty() && value.size() <= 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
  });
}
void keys(JsonObject const& object, std::set<std::wstring> const& allowed) {
  for (auto const& entry : object) if (!allowed.contains(std::wstring(entry.Key()))) throw std::runtime_error("Unknown protocol field.");
}
Request parse(std::string const& bytes) {
  if (bytes.size() > 65536) throw std::runtime_error("Message too large.");
  auto object = JsonObject::Parse(to_hstring(bytes));
  keys(object, { L"v", L"id", L"command", L"session", L"settings" });
  if (object.GetNamedNumber(L"v") != 1) throw std::runtime_error("Unsupported protocol version.");
  Request request{ to_string(object.GetNamedString(L"id")), to_string(object.GetNamedString(L"command")), "", {} };
  if (!token(request.id)) throw std::runtime_error("Invalid request ID.");
  const std::set<std::string> commands{ "hello", "getCapabilities", "selectSource", "startSession", "updateSettings", "pauseSession", "resumeSession", "stopSession", "getMetrics" };
  if (!commands.contains(request.command)) throw std::runtime_error("Unknown command.");
  if (object.HasKey(L"session")) { request.session = to_string(object.GetNamedString(L"session")); if (!token(request.session)) throw std::runtime_error("Invalid session."); }
  if (request.command != "hello" && request.command != "getCapabilities" && request.command != "selectSource" && request.session.empty()) throw std::runtime_error("Session required.");
  if (request.command == "updateSettings" && !object.HasKey(L"settings")) throw std::runtime_error("Settings required.");
  if (object.HasKey(L"settings")) {
    if (request.command != "updateSettings") throw std::runtime_error("Unexpected settings.");
    auto settings = object.GetNamedObject(L"settings"); keys(settings, { L"strength", L"denoise", L"split", L"enabled" });
    auto read = [&](wchar_t const* key, float fallback, double maximum) {
      auto value = settings.HasKey(key) ? settings.GetNamedNumber(key) : static_cast<double>(fallback);
      if (!std::isfinite(value) || value < 0 || value > maximum) throw std::runtime_error("Settings out of range.");
      return static_cast<float>(value);
    };
    request.options.strength = read(L"strength", .4f, 1); request.options.denoise = read(L"denoise", .08f, .5);
    request.options.split = read(L"split", .5f, 1);
    request.options.enabled = settings.HasKey(L"enabled") ? settings.GetNamedBoolean(L"enabled") : true;
  }
  return request;
}
std::string session_id() {
  unsigned char bytes[16]{};
  if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) throw std::runtime_error("Session entropy unavailable.");
  std::string value; constexpr char hex[] = "0123456789abcdef";
  for (auto b : bytes) { value += hex[b >> 4]; value += hex[b & 15]; } return value;
}
std::mutex outputMutex;
void send(Request const& request, bool ok, std::string const& error = "", JsonObject result = {}) {
  if (request.id.empty()) return;
  result.Insert(L"v", JsonValue::CreateNumberValue(1)); result.Insert(L"id", JsonValue::CreateStringValue(to_hstring(request.id)));
  result.Insert(L"ok", JsonValue::CreateBooleanValue(ok));
  if (!ok) result.Insert(L"error", JsonValue::CreateStringValue(to_hstring(error)));
  const auto text = to_string(result.Stringify()); const auto size = static_cast<uint32_t>(text.size());
  std::scoped_lock lock(outputMutex); DWORD written = 0;
  auto write = [&](void const* data, DWORD length) {
    auto p = static_cast<char const*>(data);
    while (length) { if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), p, length, &written, nullptr) || !written) return false; p += written; length -= written; }
    return true;
  };
  if (write(&size, sizeof(size))) write(text.data(), size);
}
constexpr char shader[] = R"HLSL(
Texture2D image : register(t0); SamplerState linearClamp : register(s0);
cbuffer Config : register(b0) { float2 texel; float strength; float denoise; float split; float enabled; float2 padding; };
struct Vertex { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex vs(uint id : SV_VertexID) { float2 p=float2((id<<1)&2,id&2); Vertex v;v.pos=float4(p*2-1,0,1);v.uv=float2(p.x,1-p.y);return v; }
float4 ps(Vertex v) : SV_TARGET {
 float4 c=image.Sample(linearClamp,v.uv);float3 a=image.Sample(linearClamp,v.uv+float2(texel.x,0)).rgb;
 float3 b=image.Sample(linearClamp,v.uv-float2(texel.x,0)).rgb;float3 d=image.Sample(linearClamp,v.uv+float2(0,texel.y)).rgb;
 float3 e=image.Sample(linearClamp,v.uv-float2(0,texel.y)).rgb;float3 blur=(a+b+d+e)*.25;
 float3 clean=lerp(c.rgb,blur,denoise*(1-smoothstep(.02,.16,length(c.rgb-blur))));
 float3 lo=min(c.rgb,min(min(a,b),min(d,e)));float3 hi=max(c.rgb,max(max(a,b),max(d,e)));
 float3 enhanced=clamp(clean+(c.rgb-blur)*strength,lo,hi);
 return float4(enabled<.5||v.uv.x<split?c.rgb:enhanced,1);
})HLSL";
class Graphics {
  com_ptr<ID3D11Device> device;
  com_ptr<ID3D11DeviceContext> context;
  com_ptr<IDXGISwapChain1> swap;
  com_ptr<ID3D11RenderTargetView> target;
  com_ptr<ID3D11VertexShader> vertex;
  com_ptr<ID3D11PixelShader> pixel;
  com_ptr<ID3D11SamplerState> sampler;
  com_ptr<ID3D11Buffer> constants;
  com_ptr<ID3D11Texture2D> copy;
  com_ptr<ID3D11ShaderResourceView> view;
  UINT sourceWidth = 0, sourceHeight = 0;
  HWND window;
public:
  IDirect3DDevice captureDevice{ nullptr };
  Graphics(HWND hwnd) : window(hwnd) {
    D3D_FEATURE_LEVEL level{};
    check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      nullptr, 0, D3D11_SDK_VERSION, device.put(), &level, context.put()));
    auto dxgi = device.as<IDXGIDevice>(); com_ptr<::IInspectable> inspectable;
    check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put())); captureDevice = inspectable.as<IDirect3DDevice>();
    com_ptr<IDXGIAdapter> adapter; check_hresult(dxgi->GetAdapter(adapter.put()));
    // Until there is an end-to-end HDR pipeline, refuse an HDR desktop rather than silently clip it.
    for (UINT i = 0;; i++) {
      com_ptr<IDXGIOutput> output; if (adapter->EnumOutputs(i, output.put()) == DXGI_ERROR_NOT_FOUND) break;
      if (auto output6 = output.try_as<IDXGIOutput6>()) {
        DXGI_OUTPUT_DESC1 desc{};
        if (SUCCEEDED(output6->GetDesc1(&desc)) && desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
          throw std::runtime_error("HDR output detected. Disable Windows HDR for this SDR-only prototype.");
      }
    }
    com_ptr<IDXGIFactory2> factory; check_hresult(adapter->GetParent(__uuidof(IDXGIFactory2), factory.put_void()));
    DXGI_SWAP_CHAIN_DESC1 desc{}; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    check_hresult(factory->CreateSwapChainForHwnd(device.get(), hwnd, &desc, nullptr, nullptr, swap.put()));
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    auto compile = [](char const* entry, char const* profile) {
      com_ptr<ID3DBlob> code, error;
      check_hresult(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, entry, profile,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.put(), error.put())); return code;
    };
    auto v = compile("vs", "vs_5_0"), p = compile("ps", "ps_5_0");
    check_hresult(device->CreateVertexShader(v->GetBufferPointer(), v->GetBufferSize(), nullptr, vertex.put()));
    check_hresult(device->CreatePixelShader(p->GetBufferPointer(), p->GetBufferSize(), nullptr, pixel.put()));
    D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    check_hresult(device->CreateSamplerState(&sd, sampler.put()));
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth = 32; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check_hresult(device->CreateBuffer(&bd, nullptr, constants.put())); resize();
  }
  void resize() {
    RECT rect{}; GetClientRect(window, &rect); if (rect.right < 1 || rect.bottom < 1) return;
    context->OMSetRenderTargets(0, nullptr, nullptr); target = nullptr;
    check_hresult(swap->ResizeBuffers(0, static_cast<UINT>(rect.right), static_cast<UINT>(rect.bottom), DXGI_FORMAT_UNKNOWN, 0));
    com_ptr<ID3D11Texture2D> back; check_hresult(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), back.put_void()));
    check_hresult(device->CreateRenderTargetView(back.get(), nullptr, target.put()));
  }
  void render(Direct3D11CaptureFrame const& frame, Options const& options) {
    auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    com_ptr<ID3D11Texture2D> texture; check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    if (desc.Width > 8192 || desc.Height > 8192) throw std::runtime_error("Capture exceeds the 8192-pixel source limit.");
    if (sourceWidth != desc.Width || sourceHeight != desc.Height) {
      view = nullptr; copy = nullptr; sourceWidth = desc.Width; sourceHeight = desc.Height;
      desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; desc.CPUAccessFlags = 0; desc.MiscFlags = 0;
      check_hresult(device->CreateTexture2D(&desc, nullptr, copy.put()));
      check_hresult(device->CreateShaderResourceView(copy.get(), nullptr, view.put()));
    }
    ID3D11ShaderResourceView* nullView = nullptr; context->PSSetShaderResources(0, 1, &nullView);
    context->CopyResource(copy.get(), texture.get()); // GPU-to-GPU, no staging/readback or browser frame transport.
    RECT rect{}; GetClientRect(window, &rect); if (rect.right < 1 || rect.bottom < 80 || !target) return;
    float availableWidth = static_cast<float>(rect.right), availableHeight = static_cast<float>(rect.bottom - 70);
    float fit = std::min(availableWidth / sourceWidth, availableHeight / sourceHeight);
    D3D11_VIEWPORT viewport{ (availableWidth - sourceWidth * fit) / 2, 70 + (availableHeight - sourceHeight * fit) / 2,
      sourceWidth * fit, sourceHeight * fit, 0, 1 };
    float clear[4]{ .04f, .07f, .09f, 1 }; context->ClearRenderTargetView(target.get(), clear);
    auto* rtv = target.get(); context->OMSetRenderTargets(1, &rtv, nullptr); context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(nullptr); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.get(), nullptr, 0); context->PSSetShader(pixel.get(), nullptr, 0);
    float data[8]{ 1.f / sourceWidth, 1.f / sourceHeight, options.strength, options.denoise, options.split, options.enabled ? 1.f : 0.f, 0, 0 };
    context->UpdateSubresource(constants.get(), 0, nullptr, data, 0, 0);
    auto* cb = constants.get(); auto* ss = sampler.get(); auto* srv = view.get();
    context->PSSetConstantBuffers(0, 1, &cb); context->PSSetSamplers(0, 1, &ss); context->PSSetShaderResources(0, 1, &srv);
    context->Draw(3, 0); context->PSSetShaderResources(0, 1, &nullView); check_hresult(swap->Present(1, 0));
  }
  void clear() {
    if (!target) return;
    float color[4]{ .04f, .07f, .09f, 1 }; context->ClearRenderTargetView(target.get(), color); swap->Present(1, 0);
    view = nullptr; copy = nullptr; sourceWidth = sourceHeight = 0;
  }
};
class App {
  std::unique_ptr<Graphics> graphics;
  GraphicsCaptureItem item{ nullptr };
  Direct3D11CaptureFramePool pool{ nullptr };
  GraphicsCaptureSession capture{ nullptr };
  Direct3D11CaptureFrame latest{ nullptr };
  Direct3D11CaptureFramePool::FrameArrived_revoker frames;
  GraphicsCaptureItem::Closed_revoker closed;
  std::mutex frameMutex;
  std::atomic<bool> framePosted = false;
  winrt::Windows::Graphics::SizeInt32 poolSize{};
  bool picking = false, active = false, paused = false, hotkey = false;
  std::string session;
  std::atomic<uint64_t> epoch = 0;
  uint64_t frameCount = 0;
  Options options;
public:
  HWND hwnd{};
  void title(std::wstring const& text) { SetWindowTextW(hwnd, (L"Shiny Engine | " + text).c_str()); }
  void setup(HWND handle) {
    hwnd = handle;
    auto button = [&](wchar_t const* text, int x, int width, UINT id) {
      CreateWindowW(L"BUTTON", text, WS_VISIBLE | WS_CHILD | WS_TABSTOP, x, 12, width, 32, hwnd,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    };
    button(L"Choose source", 12, 130, PICK_BUTTON); button(L"Pause / resume", 150, 135, PAUSE_BUTTON);
    button(L"Stop capture", 293, 120, STOP_BUTTON); button(L"Compare", 421, 95, COMPARE_BUTTON);
    CreateWindowW(L"STATIC", L"Spatial SDR preview. Not DLSS.  Ctrl+Shift+F10 = emergency stop.", WS_VISIBLE | WS_CHILD,
      14, 48, 620, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    hotkey = RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, VK_F10) != 0;
    if (!hotkey) title(L"Emergency shortcut unavailable; close the conflicting app before capture.");
    // Refuse capture if output cannot be excluded; do not create recursive display feedback.
    if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) throw std::runtime_error("Capture exclusion unavailable on this Windows session.");
  }
  void stop() {
    epoch++; active = false; paused = false;
    frames.revoke(); closed.revoke();
    if (capture) { capture.Close(); capture = nullptr; }
    if (pool) { pool.Close(); pool = nullptr; }
    { std::scoped_lock lock(frameMutex); if (latest) { latest.Close(); latest = nullptr; } framePosted = false; }
    item = nullptr; session.clear();
    if (graphics) graphics->clear();
    title(L"Capture stopped | Choose a source to begin");
  }
  fire_and_forget pick(Request request = {}) {
    if (picking) { send(request, false, "The source picker is already open."); co_return; }
    stop(); picking = true; const auto generation = epoch.load();
    try {
      if (!hotkey) throw std::runtime_error("Emergency stop shortcut is unavailable.");
      if (!GraphicsCaptureSession::IsSupported()) throw std::runtime_error("Windows Graphics Capture is not supported.");
      GraphicsCapturePicker picker; auto initialize = picker.as<IInitializeWithWindow>(); check_hresult(initialize->Initialize(hwnd));
      auto selected = co_await picker.PickSingleItemAsync();
      if (!selected || generation != epoch) throw std::runtime_error("Source selection was cancelled.");
      item = selected; session = session_id();
      closed = item.Closed(auto_revoke, [this, generation](auto const&, auto const&) { PostMessageW(hwnd, SOURCE_CLOSED, static_cast<WPARAM>(generation), 0); });
      JsonObject result; result.Insert(L"session", JsonValue::CreateStringValue(to_hstring(session)));
      send(request, true, "", result); title(L"Source selected | Awaiting explicit start");
      if (request.id.empty()) start();
    } catch (hresult_error const& error) { send(request, false, to_string(error.message())); title(std::wstring(error.message())); }
      catch (std::exception const& error) { send(request, false, error.what()); title(std::wstring(to_hstring(error.what()))); }
    picking = false;
  }
  void start() {
    if (!item || active) throw std::runtime_error("Select an inactive source first.");
    if (!graphics) graphics = std::make_unique<Graphics>(hwnd);
    poolSize = item.Size();
    if (poolSize.Width < 1 || poolSize.Height < 1 || poolSize.Width > 8192 || poolSize.Height > 8192) throw std::runtime_error("Unsupported source dimensions.");
    pool = Direct3D11CaptureFramePool::CreateFreeThreaded(graphics->captureDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, poolSize);
    const auto generation = epoch.load();
    frames = pool.FrameArrived(auto_revoke, [this, generation](auto const& sender, auto const&) {
      try {
        auto frame = sender.TryGetNextFrame(); if (!frame) return;
        bool post = false;
        { std::scoped_lock lock(frameMutex);
          if (generation != epoch.load()) { frame.Close(); return; }
          if (latest) latest.Close(); latest = frame; post = !framePosted.exchange(true);
        }
        if (post) PostMessageW(hwnd, FRAME, static_cast<WPARAM>(generation), 0);
      } catch (...) { PostMessageW(hwnd, SOURCE_CLOSED, static_cast<WPARAM>(generation), 0); }
    });
    capture = pool.CreateCaptureSession(item); capture.IsCursorCaptureEnabled(true);
    frameCount = 0; active = true; paused = false;
    try { capture.StartCapture(); } catch (...) { stop(); throw; }
    title(L"CAPTURE ACTIVE | Spatial SDR preview | Ctrl+Shift+F10 stops");
  }
  void frame(uint64_t generation) {
    if (generation != epoch) return;
    Direct3D11CaptureFrame current{ nullptr };
    { std::scoped_lock lock(frameMutex); current = latest; latest = nullptr; framePosted = false; }
    if (!current) return;
    try {
      auto size = current.ContentSize();
      if (size.Width != poolSize.Width || size.Height != poolSize.Height) {
        current.Close();
        if (size.Width < 1 || size.Height < 1 || size.Width > 8192 || size.Height > 8192) throw std::runtime_error("Source dimensions unavailable or unsupported.");
        poolSize = size; pool.Recreate(graphics->captureDevice, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, poolSize); return;
      }
      if (active && !paused && !IsIconic(hwnd)) { graphics->render(current, options); frameCount++; }
      current.Close();
    } catch (...) { current.Close(); stop(); graphics.reset(); title(L"GPU/source failure. Capture stopped; choose the source again."); }
  }
  void sourceClosed(uint64_t generation) { if (generation == epoch) stop(); }
  void resize() { if (graphics && !IsIconic(hwnd)) { try { graphics->resize(); } catch (...) { stop(); graphics.reset(); } } }
  void togglePause() { if (!active) return; paused = !paused; title(paused ? L"Preview paused | Capture remains active; Stop releases it" : L"CAPTURE ACTIVE | Spatial SDR preview"); }
  void compare() { options.split = options.split > 0 ? 0 : .5f; }
  void command(Request const& request) {
    try {
      if (request.command == "hello" || request.command == "getCapabilities") {
        JsonObject result; result.Insert(L"description", JsonValue::CreateStringValue(L"Windows SDR spatial preview. No neural backend or click-through desktop overlay."));
        result.Insert(L"neuralRendering", JsonValue::CreateBooleanValue(false)); result.Insert(L"captureSupported", JsonValue::CreateBooleanValue(GraphicsCaptureSession::IsSupported()));
        send(request, true, "", result); return;
      }
      if (request.command == "selectSource") { pick(request); return; }
      if (request.session != session || session.empty()) throw std::runtime_error("The session is no longer authorized. Select a source again.");
      if (request.command == "startSession") start();
      else if (request.command == "stopSession") stop();
      else if (request.command == "pauseSession") { paused = true; title(L"Preview paused | Capture still active"); }
      else if (request.command == "resumeSession") { paused = false; title(L"CAPTURE ACTIVE | Spatial SDR preview"); }
      else if (request.command == "updateSettings") options = request.options;
      else if (request.command == "getMetrics") {
        JsonObject result; result.Insert(L"framesPresented", JsonValue::CreateNumberValue(static_cast<double>(frameCount)));
        result.Insert(L"gpuTimingVerified", JsonValue::CreateBooleanValue(false)); send(request, true, "", result); return;
      }
      send(request, true);
    } catch (hresult_error const& error) { send(request, false, to_string(error.message())); }
      catch (std::exception const& error) { send(request, false, error.what()); }
  }
  ~App() { stop(); if (hwnd) UnregisterHotKey(hwnd, 1); }
};
LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_NCCREATE) { app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app)); }
  if (!app) return DefWindowProcW(hwnd, message, wp, lp);
  switch (message) {
    case REQUEST: { std::unique_ptr<Request> request(reinterpret_cast<Request*>(lp)); app->command(*request); return 0; }
    case FRAME: app->frame(wp); return 0;
    case SOURCE_CLOSED: app->sourceClosed(wp); return 0;
    case WM_SIZE: app->resize(); return 0;
    case WM_COMMAND:
      if (LOWORD(wp) == PICK_BUTTON) app->pick(); else if (LOWORD(wp) == STOP_BUTTON) app->stop();
      else if (LOWORD(wp) == PAUSE_BUTTON) app->togglePause(); else if (LOWORD(wp) == COMPARE_BUTTON) app->compare(); return 0;
    case WM_HOTKEY: app->stop(); return 0;
    case WM_KEYDOWN: if (wp == VK_ESCAPE) { app->stop(); return 0; } break;
    case WM_DESTROY: app->stop(); PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd, message, wp, lp);
}
bool readExact(HANDLE pipe, void* memory, DWORD size) {
  auto* position = static_cast<char*>(memory);
  while (size) { DWORD count = 0; if (!ReadFile(pipe, position, size, &count, nullptr) || count == 0) return false; position += count; size -= count; } return true;
}
void readMessages(HWND hwnd) {
  init_apartment(apartment_type::multi_threaded);
  auto pipe = GetStdHandle(STD_INPUT_HANDLE);
  for (;;) {
    uint32_t size = 0; if (!readExact(pipe, &size, 4) || size == 0 || size > 65536) break;
    std::string input(size, '\0'); if (!readExact(pipe, input.data(), size)) break;
    try {
      auto request = std::make_unique<Request>(parse(input));
      if (PostMessageW(hwnd, REQUEST, 0, reinterpret_cast<LPARAM>(request.get()))) request.release(); else break;
    } catch (...) { /* Invalid envelope never reaches privileged code. Connection remains bounded. */ }
  }
  PostMessageW(hwnd, WM_CLOSE, 0, 0);
}
int selfTest() {
  auto valid = parse(R"({"v":1,"id":"a","command":"hello"})");
  if (valid.command != "hello") return 1;
  const std::vector<std::string> invalid{
    R"({"v":2,"id":"a","command":"hello"})", R"({"v":1,"id":"a","command":"exec"})",
    R"({"v":1,"id":"a","command":"startSession"})", R"({"v":1,"id":"a","command":"hello","path":"C:/secret"})",
    R"({"v":1,"id":"a","command":"updateSettings","session":"s","settings":{"strength":9}})",
    R"({"v":1,"id":"a","command":"updateSettings","session":"s","settings":{"scale":2}})",
    R"({"v":1,"id":"bad/id","command":"hello"})", R"({"v":1,"id":"a","command":"updateSettings","session":"s"})"
  };
  for (auto const& text : invalid) { bool rejected = false; try { parse(text); } catch (...) { rejected = true; } if (!rejected) return 2; }
  auto session = session_id(); if (!token(session) || session.size() != 32) return 3;
  for (auto entry : { "vs", "ps" }) {
    com_ptr<ID3DBlob> code, error;
    auto profile = std::string(entry) == "vs" ? "vs_5_0" : "ps_5_0";
    if (FAILED(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, entry, profile,
      D3DCOMPILE_ENABLE_STRICTNESS, 0, code.put(), error.put()))) {
      if (error) std::cerr.write(static_cast<char const*>(error->GetBufferPointer()), static_cast<std::streamsize>(error->GetBufferSize()));
      return 4;
    }
  }
  std::cout << "PASS: protocol validation, secure session IDs and HLSL compilation; capture and GPU execution not exercised.\n"; return 0;
}
int wmain(int argc, wchar_t** argv) {
  try {
    init_apartment(apartment_type::single_threaded);
    if (argc == 2 && std::wstring(argv[1]) == L"--self-test") return selfTest();
    bool bridge = argc >= 2;
    if (bridge) {
      wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH);
      std::ifstream file(std::filesystem::path(path).parent_path() / "allowed-origin.txt"); std::string allowed; std::getline(file, allowed);
      auto origin = to_string(hstring(argv[1]));
      if (allowed.size() != 52 || !allowed.starts_with("chrome-extension://") || allowed.back() != '/' || origin != allowed ||
          !std::all_of(allowed.begin() + 19, allowed.end() - 1, [](char c) { return c >= 'a' && c <= 'p'; })) return 4;
    } else if (auto console = GetConsoleWindow()) ShowWindow(console, SW_HIDE);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    App app; WNDCLASSW wc{}; wc.lpfnWndProc = procedure; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ShinyEnginePreview"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) throw std::runtime_error("Window registration failed.");
    auto hwnd = CreateWindowExW(0, wc.lpszClassName, L"Shiny Engine | Choose a source", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
      CW_USEDEFAULT, CW_USEDEFAULT, 1100, 740, nullptr, nullptr, wc.hInstance, &app);
    if (!hwnd) throw std::runtime_error("Preview creation failed."); app.setup(hwnd); ShowWindow(hwnd, SW_SHOW);
    std::thread reader; if (bridge) reader = std::thread(readMessages, hwnd);
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { if (!IsDialogMessageW(hwnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    if (reader.joinable()) { CancelSynchronousIo(reader.native_handle()); reader.join(); }
    return 0;
  } catch (hresult_error const& error) { MessageBoxW(nullptr, error.message().c_str(), L"Shiny Engine: initialization failed", MB_OK | MB_ICONERROR); return 1; }
    catch (std::exception const& error) { MessageBoxW(nullptr, to_hstring(error.what()).c_str(), L"Shiny Engine: initialization failed", MB_OK | MB_ICONERROR); return 1; }
}
