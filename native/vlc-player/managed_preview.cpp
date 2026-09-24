// SPDX-License-Identifier: MIT
#include "managed_preview.hpp"
#include "../library-manager/client.hpp"
#include "../library-manager/builtins.hpp"
#include <gdiplus.h>
#include <chrono>
#include <fstream>
#include "preview_presenter.hpp"
#include "../ui/theme.hpp"
namespace shiny::player {
namespace {
using namespace packages;
class Session {
 std::mutex mutex;std::condition_variable_any changed;std::jthread thread;
 std::shared_ptr<WorkerClient> child;std::optional<NrImage> pending,complete;
 std::string message="Verifying selected package and worker...";bool ready=false,finished=false;unsigned superseded=0;
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
 void submit(NrImage frame){std::lock_guard lock(mutex);if(!ready||finished)return;if(pending)++superseded;pending=std::move(frame);changed.notify_one();}
 std::optional<NrImage> take(){std::lock_guard lock(mutex);auto result=std::move(complete);complete.reset();return result;}
 std::string status(){std::lock_guard lock(mutex);return message;}
 unsigned skipped(){std::lock_guard lock(mutex);return superseded;}
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
 HWND hwnd=nullptr;
 design::Theme theme;
 preview::Presenter canvas;
 preview::Latency latency;
 std::shared_ptr<VlcApi> api;Item item;uint32_t width,height;int64_t startTime;
 std::unique_ptr<NrSource> source;std::unique_ptr<Session> session;std::optional<NrImage> image;
 uint64_t generation=1;unsigned frames=0;bool paused=false,seekPending=false,compatibility=false;
 Impl(HWND owner,std::shared_ptr<VlcApi> a,const Item& i,uint32_t w,uint32_t h,int64_t t):api(std::move(a)),item(i),width(w),height(h),startTime(t){
  WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"ShinyManagedPreview";RegisterClassW(&wc);
  hwnd=CreateWindowExW(0,wc.lpszClassName,L"Shiny Player · Video studio",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1180,800,owner,nullptr,wc.hInstance,this);
  if(!hwnd)throw std::runtime_error("Cannot create managed preview.");ShowWindow(hwnd,SW_SHOW);
 }
 ~Impl(){stop();if(hwnd&&IsWindow(hwnd))DestroyWindow(hwnd);}
 int px(int n)const{return theme.px(n);}
 void label(const std::wstring& value){
  wchar_t existing[2048]{};GetWindowTextW(GetDlgItem(hwnd,106),existing,2048);
  if(value!=existing)SetWindowTextW(GetDlgItem(hwnd,106),value.c_str());
 }
 void button(int id,const wchar_t* title){auto child=CreateWindowExW(0,L"BUTTON",title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
  SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(theme.body),TRUE);SetWindowSubclass(child,design::hoverProc,1,0);
 }
 void create(){theme.set(hwnd);
  const std::pair<int,const wchar_t*> controls[]={{101,L"Start selected version"},{102,L"Stop preview"},{103,L"Pause preview"},{104,L"Reset to opening time"},{105,L"Libraries..."},
   {111,L"Side by side"},{112,L"Wipe compare"},{113,L"Processed only"},{114,L"Source only"},{115,L"Inspect 1:1"},{116,L"Compatibility renderer"}};
  for(auto [id,title]:controls)button(id,title);
  for(int id:{106,107,108,109}){auto child=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,1,1,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(theme.body),TRUE);}
  SetWindowTextW(GetDlgItem(hwnd,108),L"SOURCE FRAME");SetWindowTextW(GetDlgItem(hwnd,109),L"PROCESSED / LOCAL FILTER");
  canvas.create(hwnd,120);SetTimer(hwnd,1,33,nullptr);layout();enabled();
  label(L"Select a bundled library in Libraries, then start. No enhancement is active.\nIndependent local SDR comparison · Original video and audio stay untouched.");
 }
 void enabled(){EnableWindow(GetDlgItem(hwnd,102),session!=nullptr);EnableWindow(GetDlgItem(hwnd,103),session&&!session->done());EnableWindow(GetDlgItem(hwnd,104),source!=nullptr);}
 void layout(){RECT r{};GetClientRect(hwnd,&r);int logical=MulDiv(r.right,96,static_cast<int>(theme.dpi)),heightPx=static_cast<int>(r.bottom);
  int columns=logical>=970?5:3,buttonWidth=(logical-48-(columns-1)*8)/columns;
  for(int i=0;i<5;++i)MoveWindow(GetDlgItem(hwnd,101+i),px(24+(i%columns)*(buttonWidth+8)),px(102+(i/columns)*42),px(buttonWidth),px(34),TRUE);
  const int viewTop=columns==5?154:196,viewColumns=logical>=970?6:3,viewWidth=(logical-48-(viewColumns-1)*8)/viewColumns;
  for(int i=0;i<6;++i)MoveWindow(GetDlgItem(hwnd,111+i),px(24+(i%viewColumns)*(viewWidth+8)),px(viewTop+(i/viewColumns)*38),px(viewWidth),px(30),TRUE);
  int imageTop=px(viewTop+(viewColumns==6?68:106));
  MoveWindow(GetDlgItem(hwnd,108),px(24),imageTop-px(26),(r.right-px(60))/2,px(22),TRUE);
  MoveWindow(GetDlgItem(hwnd,109),(r.right+px(12))/2,imageTop-px(26),(r.right-px(60))/2,px(22),TRUE);
  MoveWindow(canvas.hwnd,px(24),imageTop,r.right-px(48),std::max(px(100),heightPx-imageTop-px(137)),TRUE);
  MoveWindow(GetDlgItem(hwnd,107),px(24),heightPx-px(122),r.right-px(48),px(24),TRUE);
  MoveWindow(GetDlgItem(hwnd,106),px(24),heightPx-px(87),r.right-px(48),px(78),TRUE);InvalidateRect(hwnd,nullptr,TRUE);
 }
 void stop(){session.reset();source.reset();image.reset();canvas.clear();latency.clear();paused=false;frames=0;++generation;
  if(hwnd&&IsWindow(hwnd)){SetWindowTextW(GetDlgItem(hwnd,103),L"Pause preview");SetWindowTextW(GetDlgItem(hwnd,107),L"No processed frame · Package integrity is checked before every session");enabled();InvalidateRect(hwnd,nullptr,TRUE);}}
 void start(){stop();source=std::make_unique<NrSource>(api,item,width,height,startTime,960);seekPending=true;session=std::make_unique<Session>();enabled();label(L"Verifying selection. No enhancement is active yet.");}
 void tick(){if(!session||!source)return;
  if(seekPending&&source->seekable()){source->seek(startTime);seekPending=false;++generation;image.reset();canvas.clear();latency.clear();frames=0;}
  if(!paused&&!seekPending)if(auto frame=source->sample()){frame->controlRevision=generation;session->submit(std::move(*frame));}
  if(auto frame=session->take())if(frame->controlRevision==generation&&!paused){
   image=std::move(frame);++frames;latency.add(image->milliseconds);canvas.frame(image->header.width,image->header.height,image->original,image->enhanced);
  }
  auto status=wide(session->status());
  if(frames&&image){status+=L"\nFrames: "+std::to_wstring(frames)+L" · Preview audio muted; not synchronized to primary playback.";
   auto metrics=std::to_wstring(image->header.width)+L" × "+std::to_wstring(image->header.height)+L" SDR  ·  "+canvas.renderer()+
    L"  ·  p95 "+std::to_wstring(static_cast<int>(latency.p95()))+L" ms worker round-trip  ·  Superseded inputs: "+std::to_wstring(session->skipped());
   SetWindowTextW(GetDlgItem(hwnd,107),metrics.c_str());
  }
  label(status);if(session->done())source->pause(true);enabled();
 }
 void draw(HDC dc){RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,theme.background);
  theme.header(dc,r.right,L"Video studio",L"Compare the same decoded frame · Local processing · No neural reconstruction or DLSS claim");
 }
 void command(int id){switch(id){
  case 101:start();break;
  case 102:stop();label(L"Preview stopped. Original playback is unaffected.");break;
  case 103:if(source){paused=!paused;++generation;source->pause(paused);SetWindowTextW(GetDlgItem(hwnd,103),paused?L"Resume preview":L"Pause preview");}break;
  case 104:if(source){++generation;image.reset();canvas.clear();seekPending=true;}break;
  case 105:openLibraryManager(hwnd);break;
  case 111:case 112:case 113:case 114:canvas.view(id-111);SetWindowTextW(GetDlgItem(hwnd,108),id==113?L"PROCESSED / LOCAL FILTER":L"SOURCE FRAME");ShowWindow(GetDlgItem(hwnd,109),id<=112?SW_SHOW:SW_HIDE);for(int c=111;c<=114;++c)InvalidateRect(GetDlgItem(hwnd,c),nullptr,FALSE);break;
  case 115:canvas.actual=!canvas.actual;SetWindowTextW(GetDlgItem(hwnd,115),canvas.actual?L"Fit to view":L"Inspect 1:1");InvalidateRect(canvas.hwnd,nullptr,FALSE);break;
  case 116:compatibility=!compatibility;canvas.software(compatibility);SetWindowTextW(GetDlgItem(hwnd,116),compatibility?L"Use Direct2D":L"Compatibility renderer");break;
 }}
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto* p=reinterpret_cast<Impl*>(GetWindowLongPtrW(h,GWLP_USERDATA));
  if(m==WM_NCCREATE){p=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
  try{switch(m){case WM_CREATE:p->create();return 0;case WM_SIZE:p->layout();return 0;
   case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={p->px(720),p->px(680)};return 0;
   case WM_DPICHANGED:{p->theme.set(h,HIWORD(w));auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);return 0;}
   case WM_SETTINGCHANGE:p->theme.set(h);p->layout();return 0;
   case WM_CTLCOLORSTATIC:return reinterpret_cast<LRESULT>(p->theme.control(reinterpret_cast<HDC>(w)));
   case WM_DRAWITEM:{auto& d=*reinterpret_cast<DRAWITEMSTRUCT*>(l);p->theme.button(d,d.CtlID==101||static_cast<int>(d.CtlID)==111+p->canvas.mode);return TRUE;}
   case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,p->theme.background);return 1;}
   case WM_TIMER:p->tick();return 0;
   case WM_COMMAND:p->command(LOWORD(w));return 0;
   case WM_PAINT:{PAINTSTRUCT ps{};auto dc=BeginPaint(h,&ps);p->draw(dc);EndPaint(h,&ps);return 0;}
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
  auto root=report.parent_path()/L"managed-test-store";auto policy=productionPolicy();std::string first,second,third;
  {Store store(root,policy);first=store.importFolder(executableDirectory()/L"library-bundles"/L"1.0.0");second=store.importFolder(executableDirectory()/L"library-bundles"/L"1.1.0");third=store.importFolder(executableDirectory()/L"library-bundles"/L"1.2.0");store.stage(first);store.activate(first,probePackage);}
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
  Bytes adaptive;
  {Store store(root,policy);store.stage(third);store.activate(third,probePackage);}
  {WorkerClient worker(root,third);adaptive=worker.process(h,frame->original);if(worker.revision()!=3)throw std::runtime_error("Adaptive revision missing.");}
  if(adaptive.size()!=frame->original.size())throw std::runtime_error("Adaptive output dimensions changed.");
  for(size_t n=3;n<adaptive.size();n+=4)if(adaptive[n]!=frame->original[n])throw std::runtime_error("Adaptive alpha changed.");
  {Store store(root,policy);store.original();}
  if(!primary.playing()&&primary.time()<=0)throw std::runtime_error("Original playback did not progress.");
  std::ofstream out(report);out<<"{\"decodedFrame\":true,\"versionsProcessed\":3,\"leasePreserved\":true,\"rollback\":true,\"originalPlaybackPreserved\":true,\"alphaPreserved\":true,\"sourceSha256\":\""<<digest(frame->original)<<"\",\"firstSha256\":\""<<digest(firstResult)<<"\",\"secondSha256\":\""<<digest(secondResult)<<"\",\"dlss\":false,\"gpuCertified\":false,\"audioDeviceCertified\":false}\n";
  return out?0:1;
 }catch(const std::exception& e){std::ofstream(report)<<"{\"error\":\"managed-playback-test-failed\"}\n";OutputDebugStringA(e.what());return 1;}
}
}
