// SPDX-License-Identifier: MIT
#include "managed_preview.hpp"
#include "../library-manager/client.hpp"
#include "../library-manager/builtins.hpp"
#include <gdiplus.h>
#include <chrono>
#include <fstream>
namespace shiny::player {
namespace {
using namespace packages;
class Session {
 std::mutex mutex;std::condition_variable_any changed;std::jthread thread;
 std::shared_ptr<WorkerClient> child;std::optional<NrImage> pending,complete;
 std::string message="Verifying selected package and worker...";bool ready=false,finished=false;
 public:
 Session(){thread=std::jthread([this](std::stop_token stop){try{
  auto root=defaultStore();std::string id;
  {Store s(root,productionPolicy());id=s.selected();}
  if(id.empty())throw std::runtime_error("No package selected. Use Tools > Enhancement libraries, import bundled, stage and select a reference version.");
  auto worker=std::make_shared<WorkerClient>(root,id,stop);
  {std::lock_guard lock(mutex);child=worker;ready=true;message="Reference version "+std::to_string(worker->revision())+" verified. Waiting for a decoded frame; not DLSS.";}
  std::uint64_t generation=0;
  while(!stop.stop_requested()){
   std::optional<NrImage> next;
   {std::unique_lock lock(mutex);changed.wait(lock,stop,[&]{return pending.has_value();});if(stop.stop_requested())break;next=std::move(pending);pending.reset();}
   if(next->controlRevision!=generation){generation=next->controlRevision;worker->reset(generation,stop);}
   wire::FrameHeader h;h.width=next->header.width;h.height=next->header.height;h.bytes=next->header.bytes;h.generation=generation;h.ptsMs=std::max<int64_t>(0,next->sourceTimeMs);
   auto began=std::chrono::steady_clock::now();next->enhanced=worker->process(h,next->original,stop);
   next->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
   {std::lock_guard lock(mutex);complete=std::move(next);message="Actual decoded frames processed by spatial reference version "+std::to_string(worker->revision())+". Independent SDR preview, not DLSS.";}
  }
 }catch(const std::exception& e){std::lock_guard lock(mutex);message=e.what();}
 std::lock_guard lock(mutex);ready=false;finished=true;child.reset();});}
 ~Session(){thread.request_stop();changed.notify_all();{std::lock_guard lock(mutex);if(child)child->cancel();}if(thread.joinable())thread.join();}
 void submit(NrImage frame){std::lock_guard lock(mutex);if(!ready||finished)return;pending=std::move(frame);changed.notify_one();}
 std::optional<NrImage> take(){std::lock_guard lock(mutex);auto result=std::move(complete);complete.reset();return result;}
 std::string status(){std::lock_guard lock(mutex);return message;}
 bool done(){std::lock_guard lock(mutex);return finished;}
};
}
void openLibraryManager(HWND){
 auto executable=packages::executableDirectory()/L"ShinyLibraryManager.exe";
 packages::FilePin selected(executable); // Lock the explicitly packaged path during process creation.
 auto command=L"\""+executable.wstring()+L"\"";STARTUPINFOW start{};start.cb=sizeof start;PROCESS_INFORMATION process{};
 if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,executable.parent_path().c_str(),&start,&process))throw std::runtime_error("Packaged library manager could not start.");
 CloseHandle(process.hThread);CloseHandle(process.hProcess);
}
struct ManagedPanel::Impl {
 HWND hwnd=nullptr;HFONT font=nullptr;std::shared_ptr<VlcApi> api;Item item;uint32_t width,height;int64_t startTime;UINT dpi=96;
 std::unique_ptr<NrSource> source;std::unique_ptr<Session> session;std::optional<NrImage> image;
 uint64_t generation=1;unsigned frames=0;bool paused=false,seekPending=false;
 Impl(HWND owner,std::shared_ptr<VlcApi> a,const Item& i,uint32_t w,uint32_t h,int64_t t):api(std::move(a)),item(i),width(w),height(h),startTime(t){
  WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"ShinyManagedPreview";RegisterClassW(&wc);
  hwnd=CreateWindowExW(0,wc.lpszClassName,L"Managed spatial preview — not DLSS",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1050,650,owner,nullptr,wc.hInstance,this);
  if(!hwnd)throw std::runtime_error("Cannot create managed preview.");ShowWindow(hwnd,SW_SHOW);
 }
 ~Impl(){stop();if(hwnd&&IsWindow(hwnd))DestroyWindow(hwnd);if(font)DeleteObject(font);}
 int px(int n)const{return MulDiv(n,static_cast<int>(dpi),96);}
 void label(const std::wstring& text){SetWindowTextW(GetDlgItem(hwnd,106),text.c_str());}
 void create(){dpi=GetDpiForWindow(hwnd);font=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  const std::pair<int,const wchar_t*> buttons[]={{101,L"Start selected version"},{102,L"Stop preview"},{103,L"Pause preview"},{104,L"Reset to opening time"},{105,L"Libraries..."}};
  for(auto [id,title]:buttons){auto child=CreateWindowExW(0,L"BUTTON",title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);}
  auto status=CreateWindowExW(0,L"STATIC",L"Choose a package in Libraries, then start. Original video and audio remain untouched.",WS_CHILD|WS_VISIBLE,0,0,1,1,hwnd,reinterpret_cast<HMENU>(106),GetModuleHandleW(nullptr),nullptr);SendMessageW(status,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
  SetTimer(hwnd,1,40,nullptr);layout();
 }
 void layout(){RECT r{};GetClientRect(hwnd,&r);int columns=r.right/px(180)>=5?5:3,buttonWidth=(r.right-px(40))/columns;
  for(int i=0;i<5;++i)MoveWindow(GetDlgItem(hwnd,101+i),px(20)+(i%columns)*buttonWidth,px(15+(i/columns)*40),buttonWidth-px(8),px(32),TRUE);
  MoveWindow(GetDlgItem(hwnd,106),px(20),r.bottom-px(74),r.right-px(40),px(64),TRUE);InvalidateRect(hwnd,nullptr,TRUE);
 }
 void stop(){session.reset();source.reset();image.reset();paused=false;frames=0;++generation;if(hwnd&&IsWindow(hwnd)){SetWindowTextW(GetDlgItem(hwnd,103),L"Pause preview");InvalidateRect(hwnd,nullptr,TRUE);}}
 void start(){stop();source=std::make_unique<NrSource>(api,item,width,height,startTime);seekPending=true;session=std::make_unique<Session>();label(L"Verifying selection. No enhancement is active yet.");}
 void tick(){if(!session||!source)return;
  if(seekPending&&source->seekable()){source->seek(startTime);seekPending=false;++generation;image.reset();}
  if(!paused&&!seekPending)if(auto frame=source->sample()){frame->controlRevision=generation;session->submit(std::move(*frame));}
  if(auto frame=session->take())if(frame->controlRevision==generation&&!paused){image=std::move(frame);++frames;InvalidateRect(hwnd,nullptr,FALSE);}
  auto status=wide(session->status());if(frames)status+=L"\nFrames: "+std::to_wstring(frames)+L" · worker round trip: "+std::to_wstring(static_cast<int>(image?image->milliseconds:0))+L" ms. Source on left, matched processed frame on right. Preview audio is muted; not synchronized to main audio.";
  label(status);if(session->done())source->pause(true);
 }
 void draw(HDC dc){RECT rect{};GetClientRect(hwnd,&rect);SetBkMode(dc,TRANSPARENT);auto old=SelectObject(dc,font);
  RECT title{px(20),px(103),rect.right-px(20),px(140)};DrawTextW(dc,L"SOURCE FRAME                                      SPATIAL REFERENCE OUTPUT",-1,&title,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
  if(image){Gdiplus::Graphics g(dc);const auto& frame=*image;int areaW=(rect.right-px(50))/2,areaH=std::max(1,static_cast<int>(rect.bottom)-px(230));
   auto drawFrame=[&](const std::vector<uint8_t>& rgba,int left){std::vector<uint8_t> bgra=rgba;for(size_t n=0;n<bgra.size();n+=4)std::swap(bgra[n],bgra[n+2]);
    Gdiplus::Bitmap bitmap(static_cast<INT>(frame.header.width),static_cast<INT>(frame.header.height),static_cast<INT>(frame.header.width*4),PixelFormat32bppARGB,bgra.data());
    double ratio=std::min(double(areaW)/frame.header.width,double(areaH)/frame.header.height);int w=static_cast<int>(frame.header.width*ratio),h=static_cast<int>(frame.header.height*ratio);g.DrawImage(&bitmap,left+(areaW-w)/2,px(148)+(areaH-h)/2,w,h);
   };drawFrame(frame.original,px(20));drawFrame(frame.enhanced,px(30)+areaW);
  }SelectObject(dc,old);
 }
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto* p=reinterpret_cast<Impl*>(GetWindowLongPtrW(h,GWLP_USERDATA));
  if(m==WM_NCCREATE){p=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
  try{switch(m){case WM_CREATE:p->create();return 0;case WM_SIZE:p->layout();return 0;
   case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={p->px(660),p->px(540)};return 0;
   case WM_DPICHANGED:{p->dpi=HIWORD(w);auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);return 0;}
   case WM_TIMER:p->tick();return 0;
   case WM_COMMAND:switch(LOWORD(w)){case 101:p->start();break;case 102:p->stop();p->label(L"Preview stopped. Original playback is unaffected.");break;case 103:if(p->source){p->paused=!p->paused;p->source->pause(p->paused);SetWindowTextW(GetDlgItem(h,103),p->paused?L"Resume preview":L"Pause preview");}break;case 104:if(p->source){++p->generation;p->image.reset();p->seekPending=true;}break;case 105:openLibraryManager(h);break;}return 0;
   case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);p->draw(dc);EndPaint(h,&ps);return 0;}
   case WM_CLOSE:p->stop();DestroyWindow(h);return 0;case WM_DESTROY:KillTimer(h,1);p->hwnd=nullptr;return 0;
  }}catch(const std::exception& e){p->stop();p->label(wide(e.what()));}return DefWindowProcW(h,m,w,l);
 }
};
ManagedPanel::ManagedPanel(HWND owner,std::shared_ptr<VlcApi> api,const Item& item,uint32_t w,uint32_t h,int64_t t):impl(std::make_unique<Impl>(owner,std::move(api),item,w,h,t)){}
ManagedPanel::~ManagedPanel()=default;
bool ManagedPanel::visible()const{return impl->hwnd&&IsWindow(impl->hwnd);}
int managedPlaybackTest(const std::filesystem::path& runtime,const std::filesystem::path& fixture,const std::filesystem::path& report){
 using namespace packages;
 try{
  auto root=report.parent_path()/L"managed-test-store";auto policy=productionPolicy();std::string first,second;
  {Store store(root,policy);first=store.importFolder(executableDirectory()/L"library-bundles"/L"1.0.0");second=store.importFolder(executableDirectory()/L"library-bundles"/L"1.1.0");store.stage(first);store.activate(first,probePackage);}
  auto api=VlcApi::load(runtime);Item item{fixture.wstring(),L"Synthetic local video",false};Engine primary(api,nullptr,true,true);primary.open(item);
  NrSource source(api,item,640,360,0);std::optional<NrImage> frame;auto deadline=GetTickCount64()+15000;
  while(GetTickCount64()<deadline&&!frame){frame=source.sample();MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(10);}
  if(!frame)throw std::runtime_error("No real decoded frame for managed preview.");
  wire::FrameHeader h;h.width=frame->header.width;h.height=frame->header.height;h.bytes=frame->header.bytes;h.ptsMs=std::max<int64_t>(0,frame->sourceTimeMs);h.generation=2;
  Bytes firstResult,secondResult;
  {WorkerClient worker(root,first);firstResult=worker.process(h,frame->original);worker.reset(3);
   Store store(root,policy);store.stage(second);store.activate(second,probePackage);
   // An already-running worker retains its lease and exact version after selection changes.
   if(worker.revision()!=1||worker.process(h,frame->original)!=firstResult)throw std::runtime_error("Active worker version changed during selection.");}
  {WorkerClient worker(root,second);secondResult=worker.process(h,frame->original);if(worker.revision()!=2)throw std::runtime_error("Wrong selected backend version.");}
  if(firstResult==secondResult||firstResult==frame->original||secondResult==frame->original)throw std::runtime_error("Reference backend did not change decoded fixture as expected.");
  for(size_t n=3;n<firstResult.size();n+=4)if(firstResult[n]!=frame->original[n]||secondResult[n]!=frame->original[n])throw std::runtime_error("Preview alpha changed.");
  {Store store(root,policy);store.rollback(probePackage);if(store.selected()!=first)throw std::runtime_error("Playback rollback failed.");store.original();}
  if(!primary.playing()&&primary.time()<=0)throw std::runtime_error("Original playback did not progress.");
  std::ofstream out(report);out<<"{\"decodedFrame\":true,\"versionsProcessed\":2,\"leasePreserved\":true,\"rollback\":true,\"originalPlaybackPreserved\":true,\"alphaPreserved\":true,\"sourceSha256\":\""<<digest(frame->original)<<"\",\"firstSha256\":\""<<digest(firstResult)<<"\",\"secondSha256\":\""<<digest(secondResult)<<"\",\"dlss\":false,\"gpuCertified\":false,\"audioDeviceCertified\":false}\n";
  return out?0:1;
 }catch(const std::exception& e){std::ofstream(report)<<"{\"error\":\"managed-playback-test-failed\"}\n";OutputDebugStringA(e.what());return 1;}
}
}
