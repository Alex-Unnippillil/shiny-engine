// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include "nr_client.hpp"
#include "../nr-worker/research.hpp"
#include <uxtheme.h>
#include <iomanip>
namespace shiny::player {
struct NeuralPanel::Impl {
 enum {Probe=101,Model=102,Start=103,Stop=104,Pause=105,Tone=106,Structure=107,Blend=108,
       Research=109,Consent=110,View=111,Divider=112,Save=113,Report=114,Reset=115};
 HWND hwnd=nullptr,info=nullptr;HFONT font=nullptr,titleFont=nullptr;UINT dpi=96;
 std::shared_ptr<VlcApi> api;std::unique_ptr<NrSource> source;std::unique_ptr<NrSession> session;
 std::filesystem::path model;std::optional<NrImage> latest,inputFrame;
 std::jthread check;std::mutex mutex;std::shared_ptr<NrProcess> probe;
 std::string checkText,modelDigest,previousStatus;bool checked=false,busy=false,validated=false,reviewed=false;
 bool research=false,resubmit=false,initialSeek=false;int64_t initial=0;uint64_t revision=1,completed=0;
 int units(int value)const{return MulDiv(value,static_cast<int>(dpi),96);}
 bool selected(int id)const{return SendMessageW(GetDlgItem(hwnd,id),BM_GETCHECK,0,0)==BST_CHECKED;}
 HWND make(const wchar_t* cls,const wchar_t* label,int id,DWORD styles=0){
  auto h=CreateWindowW(cls,label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|styles,0,0,20,20,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
  SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
  if(std::wstring_view(cls)==L"STATIC")SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)&~WS_TABSTOP);
  return h;
 }
 void setDpi(UINT value){
  dpi=value?value:96;
  auto old=font,oldTitle=titleFont;
  font=CreateFontW(-units(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  titleFont=CreateFontW(-units(23),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  if(hwnd)EnumChildWindows(hwnd,[](HWND child,LPARAM handle)->BOOL{SendMessageW(child,WM_SETFONT,static_cast<WPARAM>(handle),TRUE);return TRUE;},reinterpret_cast<LPARAM>(font));
  if(old)DeleteObject(old);if(oldTitle)DeleteObject(oldTitle);
 }
 Impl(HWND owner,std::shared_ptr<VlcApi> runtime,const Item& item,uint32_t w,uint32_t h,int64_t at):api(std::move(runtime)),initial(at){
  source=std::make_unique<NrSource>(api,item,w,h,at);
  WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=shiny::ui::backgroundBrush;wc.lpszClassName=L"ShinyNativeNeural";RegisterClassW(&wc);
  setDpi(GetDpiForWindow(owner));
  hwnd=CreateWindowExW(0,wc.lpszClassName,L"DLSS-NR Research Studio · Local / Unverified",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,units(1100),units(820),owner,nullptr,wc.hInstance,this);
  if(!hwnd){DeleteObject(font);DeleteObject(titleFont);font=nullptr;titleFont=nullptr;throw std::runtime_error("Cannot create Neural Workbench.");}
  ShowWindow(hwnd,SW_SHOW);SetTimer(hwnd,1,100,nullptr);
 }
 ~Impl(){stop();if(IsWindow(hwnd))DestroyWindow(hwnd);if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);}
 void cancelCheck(){
  if(check.joinable()){check.request_stop();{std::lock_guard lock(mutex);if(probe)probe->cancel();}check.join();}
  std::lock_guard lock(mutex);busy=false;checked=false;
 }
 void stop(){session.reset();cancelCheck();source.reset();}
 void forgetAuthorization(){
  session.reset();cancelCheck();std::lock_guard lock(mutex);validated=false;reviewed=false;modelDigest.clear();
  SendMessageW(GetDlgItem(hwnd,Consent),BM_SETCHECK,BST_UNCHECKED,0);
  if(latest)latest->enhanced.clear();++revision;completed=0;
 }
 void runCheck(bool inspect){
  cancelCheck();bool local=research;auto directory=model;
  {std::lock_guard lock(mutex);checked=false;busy=true;if(inspect){validated=false;reviewed=false;modelDigest.clear();}}
  SetWindowTextW(info,inspect?L"Validating local model data, tensor bounds and file hashes. No inference during inspection…":L"Checking Vulkan capabilities. Device creation is not an inference benchmark…");
  check=std::jthread([this,inspect,local,directory](std::stop_token token){
   std::string result,digest;bool success=false;
   try{
    auto args=inspect?(local?L"--inspect-research ":L"--inspect ")+quote(directory.wstring()):L"--probe";
    auto child=std::make_shared<NrProcess>(args);{std::lock_guard lock(mutex);probe=child;}
    if(token.stop_requested())child->cancel();result=child->readText(token,120000);success=child->succeeded();
    if(inspect&&local&&success){
     constexpr std::string_view prefix="RESEARCH_INPUT_VALIDATED\nMANIFEST_SHA256=";
     if(!result.starts_with(prefix)||result.size()<prefix.size()+65)throw std::runtime_error("Invalid research inspection response.");
     digest=result.substr(prefix.size(),64);
     if(!nrpolicy::hashText(digest)||result[prefix.size()+64]!='\n')throw std::runtime_error("Invalid model identity response.");
    }
   }catch(const std::exception& e){success=false;result=e.what();}
   std::lock_guard lock(mutex);probe.reset();checkText=std::move(result);
   if(inspect){validated=success;reviewed=success&&!local;modelDigest=std::move(digest);}checked=true;busy=false;
  });
 }
 void updateControls(){
  auto value=[&](int id){return static_cast<int>(SendMessageW(GetDlgItem(hwnd,id),TBM_GETPOS,0,0));};
  const wchar_t* labels[]={L"Tone",L"Structure",L"Neural mix"};
  for(int i=0;i<3;++i)SetWindowTextW(GetDlgItem(hwnd,200+i),(std::wstring(labels[i])+L"   "+std::to_wstring(value(Tone+i))+L"%").c_str());
  ++revision;resubmit=true;if(latest)latest->enhanced.clear();
  InvalidateRect(hwnd,nullptr,FALSE);
 }
 void applyControls(NrImage& image){
  image.controlRevision=revision;
  image.header.tone=static_cast<float>(SendMessageW(GetDlgItem(hwnd,Tone),TBM_GETPOS,0,0))/100;
  image.header.structure=static_cast<float>(SendMessageW(GetDlgItem(hwnd,Structure),TBM_GETPOS,0,0))/100;
  image.header.blend=static_cast<float>(SendMessageW(GetDlgItem(hwnd,Blend),TBM_GETPOS,0,0))/100;
 }
 void layout(){
  RECT r{};GetClientRect(hwnd,&r);int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);
  auto pos=[&](int id,int x,int y,int cw,int ch){MoveWindow(GetDlgItem(hwnd,id),units(x),units(y),units(std::max(1,cw)),units(std::max(1,ch)),TRUE);};
  pos(Research,20,64,280,26);pos(Consent,310,64,w-330,42);
  pos(Model,20,112,182,34);pos(Probe,214,112,126,34);pos(Start,352,112,168,34);pos(Stop,532,112,142,34);pos(Pause,686,112,160,34);
  pos(View,20,158,230,170);pos(Divider,270,158,std::max(130,w-620),28);pos(Save,w-320,158,138,32);pos(Report,w-170,158,150,32);
  const int column=(w-40)/3;
  for(int i=0;i<3;++i){pos(200+i,20+i*column,h-206,column-12,23);pos(Tone+i,20+i*column,h-178,column-16,28);}
  pos(Reset,20,h-138,170,28);pos(211,204,h-138,w-224,42);pos(209,20,h-90,w-40,78);InvalidateRect(hwnd,nullptr,TRUE);
 }
 void drawImage(HDC dc,const NrImage& frame,bool output,RECT area){
  const auto& data=output?frame.enhanced:frame.original;if(data.empty())return;
  auto w=frame.header.width,h=frame.header.height;std::vector<uint8_t> bgra(data.size());
  for(size_t i=0;i<data.size();i+=4){bgra[i]=data[i+2];bgra[i+1]=data[i+1];bgra[i+2]=data[i];bgra[i+3]=255;}
  double scale=std::min(double(area.right-area.left)/w,double(area.bottom-area.top)/h);
  int dw=static_cast<int>(w*scale),dh=static_cast<int>(h*scale),x=area.left+(area.right-area.left-dw)/2,y=area.top+(area.bottom-area.top-dh)/2;
  BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=w;bitmap.bmiHeader.biHeight=-static_cast<int>(h);bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;bitmap.bmiHeader.biCompression=BI_RGB;
  SetStretchBltMode(dc,HALFTONE);StretchDIBits(dc,x,y,dw,dh,0,0,w,h,bgra.data(),&bitmap,DIB_RGB_COLORS,SRCCOPY);
 }
 void paint(){
  PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,shiny::ui::backgroundBrush);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,shiny::ui::text);SelectObject(dc,titleFont);
  RECT title{units(20),units(16),r.right-units(20),units(49)};DrawTextW(dc,L"DLSS-NR Research Studio",-1,&title,DT_SINGLELINE);
  SelectObject(dc,font);SetTextColor(dc,shiny::ui::accent);RECT pill{r.right-units(332),units(22),r.right-units(20),units(50)};DrawTextW(dc,L"LOCAL MODEL / UNVERIFIED OUTPUT",-1,&pill,DT_RIGHT|DT_SINGLELINE);
  int view=static_cast<int>(SendMessageW(GetDlgItem(hwnd,View),CB_GETCURSEL,0,0));
  RECT all{units(20),units(236),r.right-units(20),r.bottom-units(224)};
  FillRect(dc,&all,shiny::ui::panelBrush);
  RECT caption{units(20),units(204),r.right-units(20),units(232)};SetTextColor(dc,shiny::ui::muted);
  DrawTextW(dc,L"MATCHED INPUT  /  EXPERIMENTAL OUTPUT     ·     independent SDR frames, not main audio playback",-1,&caption,DT_SINGLELINE|DT_END_ELLIPSIS);
  if(latest){
   if(view==0){RECT a=all,b=all;a.right=r.right/2-units(6);b.left=r.right/2+units(6);drawImage(dc,*latest,false,a);drawImage(dc,*latest,true,b);}
   else if(view==1){drawImage(dc,*latest,false,all);if(!latest->enhanced.empty()){
    auto divider=static_cast<int>(SendMessageW(GetDlgItem(hwnd,Divider),TBM_GETPOS,0,0));int x=all.left+(all.right-all.left)*divider/100;
    int saved=SaveDC(dc);IntersectClipRect(dc,x,all.top,all.right,all.bottom);drawImage(dc,*latest,true,all);RestoreDC(dc,saved);
    HPEN pen=CreatePen(PS_SOLID,units(2),shiny::ui::accent);auto old=SelectObject(dc,pen);MoveToEx(dc,x,all.top,nullptr);LineTo(dc,x,all.bottom);SelectObject(dc,old);DeleteObject(pen);
   }}else drawImage(dc,*latest,view==3,all);
  }
  if(!latest||latest->enhanced.empty()){
   RECT empty=all;if(view==0)empty.left=r.right/2+units(20);else if(view!=3&&latest){EndPaint(hwnd,&ps);return;}
   InflateRect(&empty,-units(20),-units(30));SetTextColor(dc,shiny::ui::muted);
   DrawTextW(dc,L"No neural result yet\n\n1  Select Local research\n2  Inspect an authorized model folder\n3  Acknowledge this session and prepare\n\nNo weights included. No fabricated enhancement.",-1,&empty,DT_CENTER|DT_WORDBREAK);
  }
  EndPaint(hwnd,&ps);
 }
 void tick(){
  {std::lock_guard lock(mutex);
   if(checked){SetWindowTextW(info,wide(checkText).c_str());checked=false;}
   bool permitted=validated&&(reviewed||(research&&selected(Consent)&&nrpolicy::hashText(modelDigest)));
   EnableWindow(GetDlgItem(hwnd,Start),permitted&&!busy&&!session);EnableWindow(GetDlgItem(hwnd,Probe),!busy&&!session);EnableWindow(GetDlgItem(hwnd,Model),!busy&&!session);
   EnableWindow(GetDlgItem(hwnd,Research),!busy&&!session);EnableWindow(GetDlgItem(hwnd,Consent),research&&!session&&!busy);
  }
  EnableWindow(GetDlgItem(hwnd,Save),latest&&!latest->enhanced.empty());
  EnableWindow(GetDlgItem(hwnd,Divider),SendMessageW(GetDlgItem(hwnd,View),CB_GETCURSEL,0,0)==1);
  if(!source)return;
  if(!initialSeek&&source->seekable()){source->seek(initial);initialSeek=true;}
  if(session){
   auto text=session->status();if(text!=previousStatus){previousStatus=text;SetWindowTextW(info,wide(text).c_str());}
   if(auto ready=session->take();ready&&ready->controlRevision==revision){
    latest=std::move(ready);++completed;
    auto label=L"UNVERIFIED OpenDLSS-NR frame · "+std::to_wstring(latest->header.width)+L"×"+std::to_wstring(latest->header.height)+L" · "+std::to_wstring(static_cast<int>(latest->milliseconds))+L" ms worker round-trip · "+std::to_wstring(completed)+L" returned frames. CPU transfers included; no temporal / audio synchronization.";
    SetWindowTextW(info,label.c_str());InvalidateRect(hwnd,nullptr,FALSE);
   }
   if(session->finished()){session.reset();resubmit=false;SetWindowTextW(info,(L"Worker stopped: "+wide(previousStatus)+L" Original playback remains available; correct the issue and retry.").c_str());}
  }
  if(auto next=source->sample()){
   inputFrame=std::move(next);if(!session){latest=inputFrame;InvalidateRect(hwnd,nullptr,FALSE);}else resubmit=true;
  }
  if(resubmit&&inputFrame&&session&&session->ready()){auto frame=*inputFrame;frame.enhanced.clear();applyControls(frame);session->submit(std::move(frame));resubmit=false;}
 }
 void exportFrame(){
  if(!latest||latest->enhanced.empty())throw std::runtime_error("Process a frame before exporting.");
  auto files=shiny::ui::pick(hwnd,false,false,true);if(files.empty())return;
  auto frame=*latest;std::vector<uint8_t> bgra(frame.enhanced.size());
  for(size_t i=0;i<bgra.size();i+=4){bgra[i]=frame.enhanced[i+2];bgra[i+1]=frame.enhanced[i+1];bgra[i+2]=frame.enhanced[i];bgra[i+3]=255;}
  Gdiplus::Bitmap image(frame.header.width,frame.header.height,frame.header.width*4,PixelFormat32bppARGB,bgra.data());
  UINT n=0,bytes=0;Gdiplus::GetImageEncodersSize(&n,&bytes);std::vector<uint8_t> memory(bytes);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(memory.data());
  Gdiplus::GetImageEncoders(n,bytes,encoders);bool saved=false;
  for(UINT i=0;i<n;++i)if(std::wstring_view(encoders[i].MimeType)==L"image/png"){saved=image.Save(files[0].c_str(),&encoders[i].Clsid,nullptr)==Gdiplus::Ok;break;}
  if(!saved)throw std::runtime_error("Could not save the experimental frame.");SetWindowTextW(info,L"Experimental output PNG saved. This is not an original frame or a validated representation. Retain your source.");
 }
 void exportReport(){
  auto files=shiny::ui::pick(hwnd,false,false,true,L"json");if(files.empty())return;
  std::ofstream out(files[0]);std::string digest;{std::lock_guard lock(mutex);digest=modelDigest;}
  out<<"{\n  \"schema\": 1,\n  \"application\": \"0.7.0\",\n  \"mode\": \""<<(research?"local-research":"reviewed")<<"\",\n  \"modelManifestSha256\": \""<<digest<<"\",\n  \"vendorParityVerified\": false,\n  \"returnedFrames\": "<<completed<<",\n  \"hasCurrentNeuralOutput\": "<<(latest&&!latest->enhanced.empty()?"true":"false");
  if(latest){out<<",\n  \"width\": "<<latest->header.width<<",\n  \"height\": "<<latest->header.height<<",\n  \"sourcePositionMsApproximate\": "<<latest->sourceTimeMs<<",\n  \"tone\": "<<latest->header.tone<<",\n  \"structure\": "<<latest->header.structure<<",\n  \"mix\": "<<latest->header.blend<<",\n  \"workerRoundTripMs\": "<<latest->milliseconds;}
  out<<",\n  \"scope\": \"Independent SDR frames; explicit CPU transfers; no temporal state or main-audio synchronization. User consent does not verify model licensing or output correctness.\"\n}\n";
  if(!out)throw std::runtime_error("Could not save the experiment report.");SetWindowTextW(info,L"Experiment report saved without media filenames, paths, or image payloads.");
 }
 void action(int id){
  if(id==Research){forgetAuthorization();research=selected(Research);SetWindowTextW(info,research?L"Local research selected. Choose a model folder you are authorized to use. Intake validates the files, not model ownership or output quality.":L"Reviewed-only mode selected. The curated model registry is unchanged.");}
  else if(id==Model){auto files=shiny::ui::pick(hwnd,false,true);if(!files.empty()){forgetAuthorization();model=files[0];runCheck(true);}}
  else if(id==Probe)runCheck(false);
  else if(id==Start){
   std::string digest;bool permitted=false;{std::lock_guard lock(mutex);permitted=validated&&!busy&&(reviewed||(research&&selected(Consent)&&nrpolicy::hashText(modelDigest)));digest=modelDigest;}
   if(!permitted)throw std::runtime_error("Inspect a compatible model and acknowledge authorized local research use first.");
   if(session)return;session=std::make_unique<NrSession>(model,research?digest:std::string{});previousStatus.clear();resubmit=true;completed=0;
  }else if(id==Stop){session.reset();cancelCheck();if(latest)latest->enhanced.clear();resubmit=false;SetWindowTextW(info,L"Native worker stopped. The original muted preview and main player remain available.");InvalidateRect(hwnd,nullptr,FALSE);}
  else if(id==Pause&&source){bool pause=source->playing();source->pause(pause);SetWindowTextW(GetDlgItem(hwnd,Pause),pause?L"Resume preview":L"Pause preview");}
  else if(id==Reset){for(int i=0;i<3;++i)SendMessageW(GetDlgItem(hwnd,Tone+i),TBM_SETPOS,TRUE,i==2?100:50);updateControls();}
  else if(id==Save)exportFrame();else if(id==Report)exportReport();else if(id==View)InvalidateRect(hwnd,nullptr,FALSE);
 }
 void drawButton(DRAWITEMSTRUCT* d){
  bool active=(d->CtlID==Start),disabled=(d->itemState&ODS_DISABLED)!=0;
  auto color=active&&!disabled?shiny::ui::accent:shiny::ui::panel;
  FillRect(d->hDC,&d->rcItem,shiny::ui::backgroundBrush);auto brush=CreateSolidBrush(color);auto pen=CreatePen(PS_SOLID,1,(d->itemState&ODS_FOCUS)?shiny::ui::accent:RGB(56,70,86));auto ob=SelectObject(d->hDC,brush),op=SelectObject(d->hDC,pen);
  RoundRect(d->hDC,0,0,d->rcItem.right,d->rcItem.bottom,units(10),units(10));SelectObject(d->hDC,ob);SelectObject(d->hDC,op);DeleteObject(brush);DeleteObject(pen);
  wchar_t text[128]{};GetWindowTextW(d->hwndItem,text,128);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,disabled?shiny::ui::muted:active?shiny::ui::bg:shiny::ui::text);auto old=SelectObject(d->hDC,font);DrawTextW(d->hDC,text,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(d->hDC,old);
 }
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
  auto* p=reinterpret_cast<Impl*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
  try{switch(m){
   case WM_CREATE:{
    auto choice=p->make(L"BUTTON",L"Local research (unreviewed model)",Research,BS_AUTOCHECKBOX);SetWindowTheme(choice,L"",L"");
    auto consent=p->make(L"BUTTON",L"I am authorized to use this model and accept unverified experimental output for this session.",Consent,BS_AUTOCHECKBOX|BS_MULTILINE);SetWindowTheme(consent,L"",L"");
    p->make(L"BUTTON",L"Inspect model folder",Model,BS_OWNERDRAW);p->make(L"BUTTON",L"Check GPU",Probe,BS_OWNERDRAW);p->make(L"BUTTON",L"Prepare model",Start,BS_OWNERDRAW);p->make(L"BUTTON",L"Stop / reset worker",Stop,BS_OWNERDRAW);p->make(L"BUTTON",L"Pause preview",Pause,BS_OWNERDRAW);
    auto view=p->make(L"COMBOBOX",L"Comparison layout",View,CBS_DROPDOWNLIST);for(auto label:{L"Side by side",L"Wipe comparison",L"Original input",L"Experimental output"})SendMessageW(view,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendMessageW(view,CB_SETCURSEL,0,0);
    auto split=p->make(TRACKBAR_CLASSW,L"Comparison divider",Divider,TBS_HORZ|TBS_NOTICKS);SendMessageW(split,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(split,TBM_SETPOS,TRUE,50);
    p->make(L"BUTTON",L"Save output PNG",Save,BS_OWNERDRAW);p->make(L"BUTTON",L"Experiment report",Report,BS_OWNERDRAW);p->make(L"BUTTON",L"Reset adjustments",Reset,BS_OWNERDRAW);
    for(int i=0;i<3;++i){const wchar_t* names[]={L"Tone",L"Structure",L"Neural mix"};p->make(L"STATIC",names[i],200+i);auto slider=p->make(TRACKBAR_CLASSW,names[i],Tone+i,TBS_HORZ|TBS_NOTICKS);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(slider,TBM_SETPOS,TRUE,i==2?100:50);}
    p->make(L"STATIC",L"Authorized local files only. Experimental output; original playback stays separate. No model downloads.",211);
    p->info=p->make(L"STATIC",L"Choose reviewed-only or local research. Nothing is processed until you inspect a compatible local model and explicitly prepare it. Main VLC playback is independent.",209);EnableWindow(GetDlgItem(h,Start),FALSE);EnableWindow(GetDlgItem(h,Save),FALSE);p->updateControls();return 0;
   }
   case WM_SIZE:p->layout();return 0;
   case WM_DPICHANGED:{p->setDpi(HIWORD(w));auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);p->layout();return 0;}
   case WM_PAINT:p->paint();return 0;case WM_TIMER:p->tick();return 0;case WM_DRAWITEM:p->drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
   case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetTextColor(reinterpret_cast<HDC>(w),shiny::ui::text);SetBkColor(reinterpret_cast<HDC>(w),shiny::ui::bg);return reinterpret_cast<LRESULT>(shiny::ui::backgroundBrush);
   case WM_HSCROLL:if(GetDlgCtrlID(reinterpret_cast<HWND>(l))==Divider)InvalidateRect(h,nullptr,FALSE);else p->updateControls();return 0;
   case WM_COMMAND:if(LOWORD(w)==IDCANCEL){SendMessageW(h,WM_CLOSE,0,0);return 0;}p->action(LOWORD(w));return 0;
   case WM_KEYDOWN:if(w==VK_ESCAPE){SendMessageW(h,WM_CLOSE,0,0);return 0;}break;
   case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={p->units(920),p->units(740)};return 0;
   case WM_CLOSE:KillTimer(h,1);p->stop();DestroyWindow(h);return 0;
  }}catch(const std::exception& e){p->session.reset();if(p->info)SetWindowTextW(p->info,wide(e.what()).c_str());return 0;}
  return DefWindowProcW(h,m,w,l);
 }
};
NeuralPanel::NeuralPanel(HWND owner,std::shared_ptr<VlcApi> api,const Item& item,uint32_t w,uint32_t h,int64_t t):impl(std::make_unique<Impl>(owner,std::move(api),item,w,h,t)){}
NeuralPanel::~NeuralPanel()=default;
bool NeuralPanel::visible()const{return impl&&IsWindow(impl->hwnd);}
}
