// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include "nr_client.hpp"
namespace shiny::player {
struct NeuralPanel::Impl {
 HWND hwnd=nullptr,info=nullptr;std::shared_ptr<VlcApi> api;std::unique_ptr<NrSource> source;std::unique_ptr<NrSession> session;
 std::filesystem::path model;std::optional<NrImage> latest;std::jthread check;
 std::mutex mutex;std::shared_ptr<NrProcess> probe;std::string checkText;bool checked=false,busy=false,approved=false;
 int64_t initial=0;bool initialSeek=false;HFONT font=nullptr;std::string previousStatus;
 enum{Probe=101,Model=102,Start=103,Stop=104,Pause=105,Tone=106,Structure=107,Blend=108};
 HWND make(const wchar_t* cls,const wchar_t* label,int id,DWORD styles=0){auto h=CreateWindowW(cls,label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|styles,0,0,20,20,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;}
 Impl(HWND owner,std::shared_ptr<VlcApi> runtime,const Item& item,uint32_t w,uint32_t h,int64_t at):api(std::move(runtime)),initial(at){
  source=std::make_unique<NrSource>(api,item,w,h,at);
  WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=shiny::ui::backgroundBrush;wc.lpszClassName=L"ShinyNativeNeural";RegisterClassW(&wc);
  font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  hwnd=CreateWindowExW(0,wc.lpszClassName,L"Native Neural Workbench · OpenDLSS-NR",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1100,740,owner,nullptr,wc.hInstance,this);
  if(!hwnd)throw std::runtime_error("Cannot create Neural Workbench.");ShowWindow(hwnd,SW_SHOW);SetTimer(hwnd,1,100,nullptr);
 }
 ~Impl(){stop();if(IsWindow(hwnd))DestroyWindow(hwnd);if(font)DeleteObject(font);}
 void stop(){session.reset();if(check.joinable()){check.request_stop();{std::lock_guard lock(mutex);if(probe)probe->cancel();}check.join();}source.reset();}
 void cancelCheck(){if(check.joinable()){check.request_stop();{std::lock_guard lock(mutex);if(probe)probe->cancel();}check.join();}busy=false;}
 void runCheck(bool inspect){cancelCheck();checked=false;busy=true;if(inspect)approved=false;auto directory=model;
  SetWindowTextW(info,inspect?L"Inspecting local manifest identity. No weights loaded yet…":L"Checking native Vulkan device/features. This is not an inference benchmark…");
  check=std::jthread([this,inspect,directory](std::stop_token stop){std::string result;bool success=false;try{auto child=std::make_shared<NrProcess>(inspect?L"--inspect "+quote(directory.wstring()):L"--probe");{std::lock_guard lock(mutex);probe=child;}if(stop.stop_requested())child->cancel();result=child->readText(stop);success=child->succeeded();}catch(const std::exception& e){result=e.what();}
   std::lock_guard lock(mutex);probe.reset();checkText=std::move(result);if(inspect)approved=success;checked=true;busy=false;});
 }
 void layout(){RECT r{};GetClientRect(hwnd,&r);auto pos=[&](int id,int x,int y,int w,int h){MoveWindow(GetDlgItem(hwnd,id),x,y,w,h,TRUE);};
  pos(Probe,20,66,150,34);pos(Model,182,66,170,34);pos(Start,364,66,160,34);pos(Stop,536,66,140,34);pos(Pause,688,66,150,34);
  for(int i=0;i<3;++i){pos(200+i,20+i*280,r.bottom-172,220,22);pos(Tone+i,20+i*280,r.bottom-144,250,28);}pos(209,20,r.bottom-106,r.right-40,90);InvalidateRect(hwnd,nullptr,TRUE);
 }
 void drawImage(HDC dc,const NrImage& frame,bool output,RECT area){auto& data=output?frame.enhanced:frame.original;if(data.empty())return;
  const auto w=frame.header.width,h=frame.header.height;std::vector<uint8_t> bgra(data.size());for(size_t i=0;i<data.size();i+=4){bgra[i]=data[i+2];bgra[i+1]=data[i+1];bgra[i+2]=data[i];bgra[i+3]=255;}
  double scale=std::min(double(area.right-area.left)/w,double(area.bottom-area.top)/h);int dw=static_cast<int>(w*scale),dh=static_cast<int>(h*scale);int x=area.left+(area.right-area.left-dw)/2,y=area.top+(area.bottom-area.top-dh)/2;
  BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=w;bitmap.bmiHeader.biHeight=-static_cast<int>(h);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;bitmap.bmiHeader.biCompression=BI_RGB;
  SetStretchBltMode(dc,HALFTONE);StretchDIBits(dc,x,y,dw,dh,0,0,w,h,bgra.data(),&bitmap,DIB_RGB_COLORS,SRCCOPY);
 }
 void paint(){PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,shiny::ui::backgroundBrush);SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,shiny::ui::text);
  RECT title{20,20,r.right-20,54};DrawTextW(dc,L"NATIVE NEURAL WORKBENCH  /  independent SDR preview",-1,&title,DT_SINGLELINE);
  RECT left{20,150,r.right/2-10,r.bottom-196},right{r.right/2+10,150,r.right-20,r.bottom-196};FillRect(dc,&left,shiny::ui::panelBrush);FillRect(dc,&right,shiny::ui::panelBrush);
  RECT a{20,116,r.right/2,145},b{r.right/2+10,116,r.right-20,145};DrawTextW(dc,L"MATCHED ORIGINAL INPUT",-1,&a,DT_SINGLELINE);DrawTextW(dc,L"OPENDLSS-NR OUTPUT · ONLY AFTER PREPARATION",-1,&b,DT_SINGLELINE);
  if(latest){drawImage(dc,*latest,false,left);drawImage(dc,*latest,true,right);}
  if(!latest||latest->enhanced.empty()){SetTextColor(dc,shiny::ui::muted);DrawTextW(dc,L"No neural output\n\nSelect a reviewed model package and prepare it.\nNo model weights are bundled.\n\nThis panel never substitutes a spatial filter.",-1,&right,DT_CENTER|DT_WORDBREAK);}
  EndPaint(hwnd,&ps);
 }
 void tick(){
  {std::lock_guard lock(mutex);if(checked){SetWindowTextW(info,wide(checkText).c_str());checked=false;}EnableWindow(GetDlgItem(hwnd,Start),approved&&!busy&&!session);EnableWindow(GetDlgItem(hwnd,Probe),!busy&&!session);EnableWindow(GetDlgItem(hwnd,Model),!busy&&!session);}
  if(!source)return;if(!initialSeek&&source->seekable()){source->seek(initial);initialSeek=true;}
  if(session){auto text=session->status();if(text!=previousStatus){previousStatus=text;SetWindowTextW(info,wide(text).c_str());}if(auto ready=session->take()){latest=std::move(ready);auto dims=std::to_wstring(latest->header.width)+L"×"+std::to_wstring(latest->header.height);SetWindowTextW(info,(L"OpenDLSS-NR independent frame · "+dims+L" · "+std::to_wstring(static_cast<int>(latest->milliseconds))+L" ms complete worker round-trip. CPU transfers included. No temporal or audio-sync claim.").c_str());InvalidateRect(hwnd,nullptr,FALSE);}}
  if(auto image=source->sample()){if(session&&session->ready()){image->header.tone=static_cast<float>(SendMessageW(GetDlgItem(hwnd,Tone),TBM_GETPOS,0,0))/100;image->header.structure=static_cast<float>(SendMessageW(GetDlgItem(hwnd,Structure),TBM_GETPOS,0,0))/100;image->header.blend=static_cast<float>(SendMessageW(GetDlgItem(hwnd,Blend),TBM_GETPOS,0,0))/100;session->submit(std::move(*image));}else if(!session){latest=std::move(image);InvalidateRect(hwnd,nullptr,FALSE);}}
 }
 void action(int id){
  if(id==Probe)runCheck(false);
  if(id==Model){auto paths=shiny::ui::pick(hwnd,false,true);if(!paths.empty()){model=paths[0];runCheck(true);}}
  if(id==Start){bool permitted=false;{std::lock_guard lock(mutex);permitted=approved&&!busy;}if(!permitted)throw std::runtime_error("No reviewed model identity. See native NR approval documentation.");session=std::make_unique<NrSession>(model);previousStatus.clear();}
  if(id==Stop){session.reset();cancelCheck();latest.reset();SetWindowTextW(info,L"Native checks/inference stopped. Original muted preview and main playback remain available.");}
  if(id==Pause&&source){const bool pause=source->playing();source->pause(pause);SetWindowTextW(GetDlgItem(hwnd,Pause),pause?L"Resume preview":L"Pause preview");}
 }
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto* p=reinterpret_cast<Impl*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
  try{switch(m){case WM_CREATE:
   p->make(L"BUTTON",L"Check GPU",Probe);p->make(L"BUTTON",L"Choose model folder",Model);p->make(L"BUTTON",L"Prepare neural preview",Start);p->make(L"BUTTON",L"Stop worker / check",Stop);p->make(L"BUTTON",L"Pause preview",Pause);
   for(int i=0;i<3;++i){const wchar_t* labels[]={L"Local tone",L"Local structure",L"Neural blend"};p->make(L"STATIC",labels[i],200+i);auto slider=p->make(TRACKBAR_CLASSW,labels[i],Tone+i,TBS_HORZ|TBS_NOTICKS);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(slider,TBM_SETPOS,TRUE,i==2?100:50);}
   p->info=p->make(L"STATIC",L"No trained model is approved or bundled in this release. GPU checking and local manifest inspection work now. This independent muted decoder does not replace main playback. Preview: at most 512 pixels; no temporal history.",209);EnableWindow(GetDlgItem(h,Start),FALSE);return 0;
   case WM_SIZE:p->layout();return 0;case WM_PAINT:p->paint();return 0;case WM_TIMER:p->tick();return 0;
   case WM_CTLCOLORSTATIC:SetTextColor(reinterpret_cast<HDC>(w),shiny::ui::text);SetBkColor(reinterpret_cast<HDC>(w),shiny::ui::bg);return reinterpret_cast<LRESULT>(shiny::ui::backgroundBrush);
   case WM_HSCROLL:if(p->session&&p->session->ready()&&p->latest){auto image=*p->latest;image.enhanced.clear();image.header.tone=static_cast<float>(SendMessageW(GetDlgItem(h,Tone),TBM_GETPOS,0,0))/100;image.header.structure=static_cast<float>(SendMessageW(GetDlgItem(h,Structure),TBM_GETPOS,0,0))/100;image.header.blend=static_cast<float>(SendMessageW(GetDlgItem(h,Blend),TBM_GETPOS,0,0))/100;p->session->submit(std::move(image));}return 0;
   case WM_COMMAND:p->action(LOWORD(w));return 0;case WM_KEYDOWN:if(w==VK_ESCAPE){SendMessageW(h,WM_CLOSE,0,0);return 0;}break;
   case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={920,600};return 0;
   case WM_CLOSE:KillTimer(h,1);p->stop();DestroyWindow(h);return 0;case WM_DESTROY:p->hwnd=nullptr;return 0;
  }}catch(const std::exception& e){if(p->info)SetWindowTextW(p->info,wide(e.what()).c_str());}return DefWindowProcW(h,m,w,l);
 }
};
NeuralPanel::NeuralPanel(HWND owner,std::shared_ptr<VlcApi> api,const Item& item,uint32_t w,uint32_t h,int64_t time):impl(std::make_unique<Impl>(owner,std::move(api),item,w,h,time)){}
NeuralPanel::~NeuralPanel()=default;bool NeuralPanel::visible()const{return impl&&IsWindow(impl->hwnd);}
}
