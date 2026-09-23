// SPDX-License-Identifier: MIT
#include "engine.hpp"
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <fstream>
#include <sstream>
#include <array>
using namespace shiny::player;
namespace {
constexpr COLORREF bg=RGB(16,20,28),panel=RGB(26,33,45),text=RGB(231,238,247),muted=RGB(151,165,184),accent=RGB(86,218,187);
constexpr int OPEN=101,NETWORK=102,LOCATE=103,FULLVLC=104,COMPARE=105,CLOSECOMPARE=106,SNAPSHOT=107,DIAGNOSTICS=108,FULLSCREEN=109,
 PLAY=110,STOP=111,PREV=112,NEXT=113,MUTE=114,REPEAT=115,SHUFFLE=116,LOOPA=117,LOOPB=118,LOOPCLEAR=119,
 SUBFILE=120,AUDIODELAY=121,SUBDELAY=122,FRAME=123,ABOUT=124,DLSS=125,CLEAR=126,REMOVE=127,SAVEQUEUE=128,CHAPTERPREV=129,CHAPTERNEXT=130,VSR=131,
 SEEK=201,VOLUME=202,RATE=203,QUEUE=204,FXENABLE=205,CONTRAST=206,BRIGHTNESS=207,SATURATION=208,GAMMA=209,RESETFX=210,
 AUDIOFIRST=4000,SUBFIRST=5000,EQFIRST=6000,CHAPTERFIRST=7000,ASPECTFIRST=8000,CROPFIRST=8100,DEINTFIRST=8200;
const wchar_t* aspects[]={L"Source ratio",L"16:9",L"4:3",L"21:9",L"1:1"};
const wchar_t* deinterlace[]={L"Off",L"Automatic / Yadif",L"Blend",L"Bob"};
HBRUSH backgroundBrush=nullptr,panelBrush=nullptr;
struct Input {HWND edit=nullptr;bool done=false;std::optional<std::wstring> result;std::wstring label,initial;};
LRESULT CALLBACK inputProc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto* p=reinterpret_cast<Input*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(m==WM_NCCREATE){p=static_cast<Input*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
 if(!p)return DefWindowProcW(h,m,w,l);
 if(m==WM_CREATE){
  CreateWindowW(L"STATIC",p->label.c_str(),WS_CHILD|WS_VISIBLE,16,14,548,44,h,nullptr,nullptr,nullptr);
  p->edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",p->initial.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,16,62,548,26,h,reinterpret_cast<HMENU>(10),nullptr,nullptr);
  SendMessageW(p->edit,EM_SETLIMITTEXT,32760,0);
  CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,354,106,98,30,h,reinterpret_cast<HMENU>(IDCANCEL),nullptr,nullptr);
  CreateWindowW(L"BUTTON",L"Apply",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,466,106,98,30,h,reinterpret_cast<HMENU>(IDOK),nullptr,nullptr);
  SetFocus(p->edit);return 0;
 }
 if(m==WM_COMMAND && (LOWORD(w)==IDOK||LOWORD(w)==IDCANCEL)){
  if(LOWORD(w)==IDOK){int n=GetWindowTextLengthW(p->edit);std::wstring s(static_cast<size_t>(n)+1,L'\0');GetWindowTextW(p->edit,s.data(),n+1);s.resize(n);p->result=s;}
  p->done=true;DestroyWindow(h);return 0;
 }
 if(m==WM_CLOSE){p->done=true;DestroyWindow(h);return 0;}
 return DefWindowProcW(h,m,w,l);
}
std::optional<std::wstring> input(HWND parent,const wchar_t* title,const wchar_t* label,const wchar_t* initial=L""){
 Input p;p.label=label;p.initial=initial;
 RECT r{};GetWindowRect(parent,&r);
 EnableWindow(parent,FALSE);
 HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,L"ShinyPlayerInput",title,WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,
  r.left+70,r.top+80,600,190,parent,nullptr,GetModuleHandleW(nullptr),&p);
 if(!h){EnableWindow(parent,TRUE);throw std::runtime_error("Could not open dialog.");}
 MSG msg{};while(!p.done){BOOL rc=GetMessageW(&msg,nullptr,0,0);if(rc<=0){if(rc==0)PostQuitMessage(static_cast<int>(msg.wParam));break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetForegroundWindow(parent);return p.result;
}
std::vector<std::filesystem::path> pick(HWND parent,bool multiple=false,bool folder=false,bool save=false,const wchar_t* ext=L"png"){
 IFileDialog* dialog=nullptr;
 HRESULT hr=save?CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)):CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog));
 if(FAILED(hr))throw std::runtime_error("Windows file picker unavailable.");
 struct Release{IFileDialog* p;~Release(){p->Release();}} release{dialog};
 DWORD flags=FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR;
 if(folder)flags|=FOS_PICKFOLDERS;else if(!save)flags|=FOS_FILEMUSTEXIST;
 if(multiple)flags|=FOS_ALLOWMULTISELECT;
 if(save){flags|=FOS_OVERWRITEPROMPT;dialog->SetDefaultExtension(ext);std::wstring name=L"shiny-frame."+std::wstring(ext);dialog->SetFileName(name.c_str());}
 dialog->SetOptions(flags);if(dialog->Show(parent)!=S_OK)return {};
 std::vector<std::filesystem::path> out;
 auto append=[&](IShellItem* item){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){out.emplace_back(path);CoTaskMemFree(path);}};
 if(multiple){IFileOpenDialog* open=nullptr;if(SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&open)))){IShellItemArray* items=nullptr;if(SUCCEEDED(open->GetResults(&items))){DWORD n=0;items->GetCount(&n);for(DWORD i=0;i<std::min<DWORD>(n,1000);++i){IShellItem* item=nullptr;if(SUCCEEDED(items->GetItemAt(i,&item))){append(item);item->Release();}}items->Release();}open->Release();}}
 else{IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){append(item);item->Release();}}
 return out;
}
bool saveWindow(HWND h,const std::filesystem::path& path){
 RECT r{};GetClientRect(h,&r);HDC screen=GetDC(h),mem=CreateCompatibleDC(screen);HBITMAP bmp=CreateCompatibleBitmap(screen,r.right,r.bottom);
 auto old=SelectObject(mem,bmp);BOOL ok=PrintWindow(h,mem,PW_CLIENTONLY|2);SelectObject(mem,old);DeleteDC(mem);ReleaseDC(h,screen);
 if(!ok){DeleteObject(bmp);return false;}
 Gdiplus::Bitmap bitmap(bmp,nullptr);CLSID encoder{};UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> bytes(size);
 auto* info=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());Gdiplus::GetImageEncoders(count,size,info);
 for(UINT i=0;i<count;++i)if(std::wstring(info[i].MimeType)==L"image/png")encoder=info[i].Clsid;
 auto result=bitmap.Save(path.c_str(),&encoder,nullptr);DeleteObject(bmp);return result==Gdiplus::Ok;
}
struct Window {
 HWND hwnd=nullptr,video=nullptr,treatmentVideo=nullptr,queueBox=nullptr,seekBar=nullptr,volumeBar=nullptr,rateBox=nullptr,status=nullptr,primaryLabel=nullptr,secondaryLabel=nullptr;
 HFONT font=nullptr,titleFont=nullptr; HMENU audioMenu=nullptr,subMenu=nullptr,eqMenu=nullptr,chapterMenu=nullptr;
 std::vector<HWND> controls;std::vector<std::pair<int,std::wstring>> audioTracks,subTracks;
 std::shared_ptr<VlcApi> api;std::unique_ptr<Engine> engine,treatment;
 Queue queue;LoopRange loop;Effects adjustments;std::filesystem::path runtimeFolder;
 bool fullscreen=false,mutedAudio=false,seeking=false,endHandled=false,fxApplied=false,syncPending=false;
 bool driverSuper=false;
 int volume=80;float playbackRate=1;int eqIndex=-1;int64_t audioDelay=0,subtitleDelay=0;
 ULONGLONG lastSync=0,started=0;WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};LONG_PTR oldStyle=0;HMENU menu=nullptr;
 std::filesystem::path smokeImage;bool smoke=false;int smokeExit=0;
 HWND control(const wchar_t* cls,const wchar_t* name,int id,DWORD extra=0){
  HWND h=CreateWindowExW(0,cls,name,WS_CHILD|WS_VISIBLE|WS_TABSTOP|extra,0,0,10,10,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
  SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);controls.push_back(h);return h;
 }
 HWND button(const wchar_t* name,int id){return control(L"BUTTON",name,id,BS_OWNERDRAW);}
 HWND label(const wchar_t* name,int id){return control(L"STATIC",name,id,SS_LEFT);}
 void move(int id,int x,int y,int w,int h){if(auto c=GetDlgItem(hwnd,id))MoveWindow(c,x,y,std::max(1,w),std::max(1,h),TRUE);}
 void message(const std::wstring& value){SetWindowTextW(status,value.c_str());}
 void failure(const std::exception& e){message(wide(e.what()));if(!smoke)MessageBoxW(hwnd,wide(e.what()).c_str(),L"Shiny Player",MB_OK|MB_ICONWARNING);}
 void createMenus(){
  menu=CreateMenu();auto media=CreatePopupMenu(),playback=CreatePopupMenu(),audio=CreatePopupMenu(),v=CreatePopupMenu(),sub=CreatePopupMenu(),tools=CreatePopupMenu();
  auto add=[](HMENU m,int id,const wchar_t* s){AppendMenuW(m,MF_STRING,static_cast<UINT_PTR>(id),s);};
  add(media,OPEN,L"Open files...\tCtrl+O");add(media,NETWORK,L"Open network stream...\tCtrl+N");add(media,LOCATE,L"Locate installed VLC...");
  add(media,REMOVE,L"Remove selected queue item");add(media,CLEAR,L"Clear queue and stop");add(media,SAVEQUEUE,L"Export queue as M3U8...");add(media,FULLVLC,L"Open full VLC interface");
  add(playback,PLAY,L"Play / Pause\tSpace");add(playback,STOP,L"Stop\tS");add(playback,PREV,L"Previous item\tP");add(playback,NEXT,L"Next item\tN");add(playback,FRAME,L"Next frame\tE");
  add(playback,REPEAT,L"Repeat current item");add(playback,SHUFFLE,L"Shuffle queue");add(playback,LOOPA,L"Set loop A\t[");add(playback,LOOPB,L"Set loop B\t]");add(playback,LOOPCLEAR,L"Clear A-B loop");
  chapterMenu=CreatePopupMenu();AppendMenuW(playback,MF_POPUP,reinterpret_cast<UINT_PTR>(chapterMenu),L"Chapters");add(playback,CHAPTERPREV,L"Previous chapter");add(playback,CHAPTERNEXT,L"Next chapter");
  add(audio,MUTE,L"Mute\tM");audioMenu=CreatePopupMenu();AppendMenuW(audio,MF_POPUP,reinterpret_cast<UINT_PTR>(audioMenu),L"Audio track");
  eqMenu=CreatePopupMenu();AppendMenuW(audio,MF_POPUP,reinterpret_cast<UINT_PTR>(eqMenu),L"Equalizer presets");add(audio,AUDIODELAY,L"Audio delay (milliseconds)...");
  auto aspect=CreatePopupMenu(),crop=CreatePopupMenu(),deint=CreatePopupMenu();for(int i=0;i<5;++i){add(aspect,ASPECTFIRST+i,aspects[i]);add(crop,CROPFIRST+i,aspects[i]);}
  for(int i=0;i<4;++i)add(deint,DEINTFIRST+i,deinterlace[i]);AppendMenuW(v,MF_POPUP,reinterpret_cast<UINT_PTR>(aspect),L"Aspect ratio");AppendMenuW(v,MF_POPUP,reinterpret_cast<UINT_PTR>(crop),L"Crop");AppendMenuW(v,MF_POPUP,reinterpret_cast<UINT_PTR>(deint),L"Deinterlace");add(v,VSR,L"Request driver super resolution on next open (not DLSS)");add(v,FULLSCREEN,L"Full screen\tF");add(v,SNAPSHOT,L"Save displayed frame PNG...");
  add(sub,SUBFILE,L"Load subtitle file...");subMenu=CreatePopupMenu();AppendMenuW(sub,MF_POPUP,reinterpret_cast<UINT_PTR>(subMenu),L"Subtitle track");add(sub,SUBDELAY,L"Subtitle delay (milliseconds)...");
  add(tools,COMPARE,L"Compare an externally processed video...");add(tools,CLOSECOMPARE,L"Close comparison");add(tools,DLSS,L"DLSS 5 integration status");add(tools,DIAGNOSTICS,L"Save playback diagnostics...");add(tools,ABOUT,L"About / keyboard shortcuts");
  for(auto entry:{std::pair{media,L"Media"},std::pair{playback,L"Playback"},std::pair{audio,L"Audio"},std::pair{v,L"Video"},std::pair{sub,L"Subtitles"},std::pair{tools,L"Tools"}})AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(entry.first),entry.second);
  SetMenu(hwnd,menu);
 }
 void create(){
  font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  titleFont=CreateFontW(-24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  createMenus();button(L"Open media",OPEN);button(L"Network stream",NETWORK);button(L"Compare video",COMPARE);button(L"Full VLC",FULLVLC);button(L"Full screen",FULLSCREEN);
  primaryLabel=label(L"SOURCE / VLC LIVE ADJUSTMENTS",300);secondaryLabel=label(L"IMPORTED TREATMENT - PROVENANCE UNVERIFIED",301);
  video=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
  treatmentVideo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
  label(L"PLAY QUEUE",302);queueBox=control(L"LISTBOX",L"",QUEUE,LBS_NOTIFY|WS_VSCROLL|LBS_NOINTEGRALHEIGHT);button(L"Clear",CLEAR);
  control(L"BUTTON",L"Live VLC adjustments (not DLSS)",FXENABLE,BS_AUTOCHECKBOX);
  const int ids[]={CONTRAST,BRIGHTNESS,SATURATION,GAMMA};const wchar_t* names[]={L"Contrast",L"Brightness",L"Saturation",L"Gamma"};
  for(int i=0;i<4;++i){label(names[i],310+i);auto s=control(TRACKBAR_CLASSW,L"",ids[i],TBS_HORZ|TBS_NOTICKS);SendMessageW(s,TBM_SETRANGE,TRUE,MAKELPARAM(10,300));SendMessageW(s,TBM_SETPOS,TRUE,100);}
  button(L"Reset adjustments",RESETFX);button(L"DLSS 5 status",DLSS);
  label(L"00:00 / --:--",303);seekBar=control(TRACKBAR_CLASSW,L"",SEEK,TBS_HORZ|TBS_NOTICKS);SendMessageW(seekBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));
  button(L"Play",PLAY);button(L"Stop",STOP);button(L"Previous",PREV);button(L"Next",NEXT);button(L"Save frame",SNAPSHOT);
  rateBox=control(L"COMBOBOX",L"",RATE,CBS_DROPDOWNLIST);for(auto s:{L"0.5x",L"0.75x",L"1x",L"1.25x",L"1.5x",L"2x"})SendMessageW(rateBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(rateBox,CB_SETCURSEL,2,0);
  button(L"A",LOOPA);button(L"B",LOOPB);button(L"Clear A-B",LOOPCLEAR);button(L"Mute",MUTE);
  volumeBar=control(TRACKBAR_CLASSW,L"",VOLUME,TBS_HORZ|TBS_NOTICKS);SendMessageW(volumeBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(volumeBar,TBM_SETPOS,TRUE,volume);
  status=label(L"Loading installed VLC...",304);
  BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));DragAcceptFiles(hwnd,TRUE);SetTimer(hwnd,1,150,nullptr);
  try{loadRuntime(runtimeFolder.empty()?installedVlc():runtimeFolder);}catch(const std::exception& e){message(wide(e.what())+L"  Media > Locate installed VLC.");}
  started=GetTickCount64();layout();
 }
 void loadRuntime(const std::filesystem::path& folder){
  if(api)throw std::runtime_error("Restart the player to change VLC installations.");
  auto next=VlcApi::load(folder);auto nextEngine=std::make_unique<Engine>(next,video);
  engine=std::move(nextEngine);api=std::move(next);message(L"Ready · libVLC "+wide(api->runtimeVersion)+L" · local processing · DLSS 5 unavailable");
 }
 void layout(){
  RECT r{};GetClientRect(hwnd,&r);int w=r.right,h=r.bottom;
  move(OPEN,16,55,116,32);move(NETWORK,140,55,132,32);move(COMPARE,280,55,140,32);move(FULLVLC,428,55,100,32);move(FULLSCREEN,w-144,55,128,32);
  const int side=284,x=w-side,viewH=std::max(140,h-274),viewW=std::max(100,x-32);int first=treatment?viewW/2-4:viewW;
  move(300,16,104,first,20);MoveWindow(video,16,128,first,viewH,TRUE);
  ShowWindow(treatmentVideo,treatment?SW_SHOW:SW_HIDE);ShowWindow(secondaryLabel,treatment?SW_SHOW:SW_HIDE);
  if(treatment){move(301,24+first,104,first,20);MoveWindow(treatmentVideo,24+first,128,viewW-first-8,viewH,TRUE);}
  move(302,x,104,160,24);move(CLEAR,x+188,99,76,26);
  int listH=std::max(88,viewH-246);move(QUEUE,x,128,264,listH);int y=136+listH;
  move(FXENABLE,x,y,264,24);y+=29;
  for(int i=0;i<4;++i){move(310+i,x,y+3,90,24);move(CONTRAST+i,x+86,y,178,26);y+=32;}
  move(RESETFX,x,y+2,264,28);move(DLSS,x,y+37,264,28);
  move(303,16,h-126,w-32,22);move(SEEK,8,h-104,w-16,24);
  move(PLAY,16,h-69,90,32);move(STOP,112,h-69,65,32);move(PREV,184,h-69,85,32);move(NEXT,276,h-69,65,32);move(SNAPSHOT,348,h-69,104,32);
  move(RATE,460,h-67,75,180);move(LOOPA,542,h-69,32,32);move(LOOPB,580,h-69,32,32);move(LOOPCLEAR,620,h-69,86,32);
  move(MUTE,w-236,h-69,70,32);move(VOLUME,w-160,h-65,145,28);move(304,16,h-28,w-32,22);
  InvalidateRect(hwnd,nullptr,TRUE);
 }
 void addFiles(const std::vector<std::filesystem::path>& files){
  size_t first=queue.items.size();
  for(auto& p:files){auto full=std::filesystem::absolute(p);if(!std::filesystem::is_regular_file(full))continue;queue.add({full.wstring(),full.filename().wstring(),false});SendMessageW(queueBox,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(queue.items.back().title.c_str()));}
  if(queue.items.size()>first)select(first);
 }
 void closeComparison(){if(treatment){treatment.reset();syncPending=false;layout();if(engine){engine->effects=adjustments;engine->applyEffects();}}}
 void select(size_t index){
  if(!engine)throw std::runtime_error("Install or locate VLC first.");
  if(index>=queue.items.size())return;
  closeComparison();if(engine->driverSuperResolutionRequested!=driverSuper){auto replacement=std::make_unique<Engine>(api,video,false,false,driverSuper);engine=std::move(replacement);}engine->open(queue.items[index]);queue.choose(index);loop.clear();endHandled=false;fxApplied=false;
  engine->api->SetRate(engine->player,playbackRate);engine->api->SetVolume(engine->player,volume);engine->api->SetMute(engine->player,mutedAudio?1:0);engine->equalizer(eqIndex);
  SendMessageW(queueBox,LB_SETCURSEL,index,0);SetWindowTextW(hwnd,(L"Shiny Player — "+queue.items[index].title).c_str());message(L"Opening media with VLC...");
 }
 void playPause(){if(!engine)return;bool pause=engine->playing();auto st=api->State(engine->player);if(st==libvlc_Stopped||st==libvlc_Ended||st==libvlc_NothingSpecial){if(queue.selected)select(*queue.selected);else if(!queue.items.empty())select(0);return;}engine->pause(pause);if(treatment)treatment->pause(pause);}
 void seek(int64_t t){if(!engine)return;if(!engine->seek(t)){message(L"This source is not seekable.");return;}if(treatment)treatment->seek(t);}
 void fullscreenToggle(){
  if(!fullscreen){placement.length=sizeof(placement);GetWindowPlacement(hwnd,&placement);oldStyle=GetWindowLongPtrW(hwnd,GWL_STYLE);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);SetMenu(hwnd,nullptr);SetWindowLongPtrW(hwnd,GWL_STYLE,oldStyle&~WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);fullscreen=true;}
  else{SetWindowLongPtrW(hwnd,GWL_STYLE,oldStyle);SetMenu(hwnd,menu);SetWindowPlacement(hwnd,&placement);SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);fullscreen=false;}layout();
 }
 void fullVlc(){
  if(!api)throw std::runtime_error("Locate an official VLC installation first.");
  const auto exe=(api->root/L"vlc.exe").wstring();std::wstring command=quote(exe)+L" --no-one-instance";
  if(queue.selected){auto& item=queue.items[*queue.selected];if(!item.remote && engine && engine->time()>0)command+=L" --start-time="+std::to_wstring(engine->time()/1000);command+=L" -- "+quote(item.source);}
  STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
  if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,api->root.c_str(),&si,&pi))throw std::runtime_error("Could not launch the installed VLC interface.");
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);if(engine)engine->pause(true);if(treatment)treatment->pause(true);message(L"Full VLC opened separately. Shiny playback paused to prevent duplicate audio.");
 }
 void refreshMenu(HMENU m){
  auto clear=[&]{while(GetMenuItemCount(m)>0)DeleteMenu(m,0,MF_BYPOSITION);};
  if(m==audioMenu||m==subMenu){clear();auto& tracks=m==audioMenu?audioTracks:subTracks;tracks=engine?engine->tracks(m==audioMenu):std::vector<std::pair<int,std::wstring>>{};
   int current=engine?(m==audioMenu?api->GetAudioTrack(engine->player):api->GetSubtitle(engine->player)):-99;
   for(size_t i=0;i<std::min<size_t>(999,tracks.size());++i)AppendMenuW(m,MF_STRING|(tracks[i].first==current?MF_CHECKED:0),(m==audioMenu?AUDIOFIRST:SUBFIRST)+i,tracks[i].second.c_str());
   if(tracks.empty())AppendMenuW(m,MF_STRING|MF_GRAYED,0,L"No tracks available yet");
  }else if(m==eqMenu){clear();AppendMenuW(m,MF_STRING|(eqIndex<0?MF_CHECKED:0),EQFIRST,L"Off");if(api)for(unsigned i=0;i<std::min<unsigned>(998,api->EqCount());++i)AppendMenuW(m,MF_STRING|(eqIndex==static_cast<int>(i)?MF_CHECKED:0),EQFIRST+1+i,wide(api->EqName(i)).c_str());
  }else if(m==chapterMenu){clear();int n=engine?api->ChapterCount(engine->player):0;for(int i=0;i<std::min(n,999);++i)AppendMenuW(m,MF_STRING|(api->GetChapter(engine->player)==i?MF_CHECKED:0),CHAPTERFIRST+i,(L"Chapter "+std::to_wstring(i+1)).c_str());if(n<=0)AppendMenuW(m,MF_STRING|MF_GRAYED,0,L"No chapters in this source");}
 }
 void effectsFromControls(){
  adjustments.enabled=SendMessageW(GetDlgItem(hwnd,FXENABLE),BM_GETCHECK,0,0)==BST_CHECKED;
  adjustments.contrast=static_cast<float>(SendMessageW(GetDlgItem(hwnd,CONTRAST),TBM_GETPOS,0,0))/100;
  adjustments.brightness=static_cast<float>(SendMessageW(GetDlgItem(hwnd,BRIGHTNESS),TBM_GETPOS,0,0))/100;
  adjustments.saturation=static_cast<float>(SendMessageW(GetDlgItem(hwnd,SATURATION),TBM_GETPOS,0,0))/100;
  adjustments.gamma=static_cast<float>(SendMessageW(GetDlgItem(hwnd,GAMMA),TBM_GETPOS,0,0))/100;
  if(engine){engine->effects=adjustments;if(treatment)engine->effects.enabled=false;engine->applyEffects();}
  message(treatment?L"Comparison preserves the source. Adjustments resume after closing comparison.":L"Live VLC image adjustments. These are conventional filters, not DLSS or neural rendering.");
 }
 void diagnostics(const std::filesystem::path& path){
  unsigned width=0,height=0;libvlc_media_stats_t stats{};if(engine){api->VideoSize(engine->player,0,&width,&height);if(engine->media)api->GetStats(engine->media,&stats);}
  std::ofstream out(path);out<<"{\n  \"schema\":1,\n  \"app\":\"0.5.0\",\n  \"runtime\":\""<<(api?api->runtimeVersion:"not loaded")<<"\",\n  \"width\":"<<width<<",\n  \"height\":"<<height<<",\n  \"decodedVideoFrames\":"<<stats.i_decoded_video<<",\n  \"displayedFrames\":"<<stats.i_displayed_pictures<<",\n  \"lostPictures\":"<<stats.i_lost_pictures<<",\n  \"importedComparison\":"<<(treatment?"true":"false")<<",\n  \"dlss5Inference\":false,\n  \"limitation\":\"No model inference, hardware speed certification or frame-exact dual-player sync. No paths or URLs included.\"\n}\n";
  if(!out)throw std::runtime_error("Could not save diagnostics.");
 }
 void action(int id){
  if(id==OPEN){addFiles(pick(hwnd,true));return;}
  if(id==LOCATE){auto files=pick(hwnd,false,true);if(!files.empty())loadRuntime(files[0]);return;}
  if(id==NETWORK){auto s=input(hwnd,L"Open network media",L"Explicit HTTP(S), RTSP, RTP, UDP or SRT media URL. No page scraping.");if(s){if(!network(*s))throw std::runtime_error("Enter a network URL, not a local path.");queue.add({*s,L"Network stream",true});SendMessageW(queueBox,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Network stream"));select(queue.items.size()-1);}return;}
  if(id==FULLSCREEN){fullscreenToggle();return;}
  if(id==VSR){driverSuper=!driverSuper;CheckMenuItem(menu,VSR,MF_BYCOMMAND|(driverSuper?MF_CHECKED:MF_UNCHECKED));message(driverSuper?L"Driver super resolution requested for the NEXT media open. Uses VLC D3D11; hardware success unverified. Not DLSS 5.":L"Automatic VLC scaling selected for the NEXT media open.");return;}
  if(id==DLSS){MessageBoxW(hwnd,L"DLSS 5 neural rendering is NOT enabled in this release.\n\nThis native player uses VLC/libVLC for playback and conventional live image adjustments. It does not contain a native DLSS 5 inference adapter, approved model, or NVIDIA runtime.\n\nYou can load a video processed by your separately installed renderer and compare it against the source. Imported provenance is unverified. The existing experimental browser Neural Lab remains separately gated.\n\nNo leaked DLLs or models are downloaded. The separate Video menu can request VLC driver super resolution on the next media open. This can use RTX VSR on supported NVIDIA systems; driver success is not verified by this app. It is NOT DLSS 5.",L"DLSS 5 — unavailable",MB_OK|MB_ICONINFORMATION);return;}
  if(id==ABOUT){MessageBoxW(hwnd,L"Shiny Player 0.5.0 — powered by libVLC\nIndependent of VideoLAN and NVIDIA.\n\nSpace: play/pause  ·  S: stop  ·  E: next frame\nLeft/Right: seek 5 s  ·  Shift+Left/Right: 30 s\nN/P: next/previous  ·  M: mute  ·  F: fullscreen\n[ / ]: set A/B loop  ·  Esc: leave fullscreen or stop\nCtrl+O: files  ·  Ctrl+N: network stream\n\nFull VLC opens the original installed interface for disc/device capture, conversion, casting, advanced filters and other features not duplicated here. The custom player is not full VLC UI parity.\n\nlibVLC uses LGPL-2.1-or-later; VLC is GPL-2.0-or-later (some dependency builds GPL-3.0). See THIRD_PARTY_NOTICES.md. No media history or remote telemetry is stored.",L"About Shiny Player",MB_OK);return;}
  if(!engine)throw std::runtime_error("Install official 64-bit VLC 3.0.24+ in the 3.0 series, then use Media > Locate installed VLC.");
  if(id==PLAY){playPause();return;}
  if(id==STOP){closeComparison();engine->stop();loop.clear();endHandled=true;message(L"Playback stopped.");return;}
  if(id==PREV||id==NEXT){auto n=id==PREV?queue.previous():queue.next();if(n)select(*n);return;}
  if(id==CLEAR){closeComparison();engine->stop();queue.clear();loop.clear();endHandled=true;SendMessageW(queueBox,LB_RESETCONTENT,0,0);SetWindowTextW(hwnd,L"Shiny Player");message(L"Queue cleared. No media history is retained.");return;}
  if(id==REMOVE){auto i=SendMessageW(queueBox,LB_GETCURSEL,0,0);if(i==LB_ERR)return;if(queue.selected==static_cast<size_t>(i)){closeComparison();engine->stop();endHandled=true;loop.clear();}queue.erase(static_cast<size_t>(i));SendMessageW(queueBox,LB_DELETESTRING,i,0);return;}
  if(id==SAVEQUEUE){if(queue.items.empty())throw std::runtime_error("The queue is empty.");if(MessageBoxW(hwnd,L"The exported playlist includes the selected file paths and network URLs, which may contain private information. Save it?",L"Export playlist",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;auto f=pick(hwnd,false,false,true,L"m3u8");if(!f.empty()){std::ofstream out(f[0],std::ios::binary);out<<"#EXTM3U\n";for(auto& item:queue.items)out<<utf8(item.source)<<'\n';if(!out)throw std::runtime_error("Cannot export queue.");}return;}
  if(id==MUTE){mutedAudio=!mutedAudio;api->SetMute(engine->player,mutedAudio);SetWindowTextW(GetDlgItem(hwnd,MUTE),mutedAudio?L"Unmute":L"Mute");return;}
  if(id==REPEAT||id==SHUFFLE){bool& value=id==REPEAT?queue.repeat:queue.shuffle;value=!value;CheckMenuItem(menu,id,MF_BYCOMMAND|(value?MF_CHECKED:MF_UNCHECKED));return;}
  if(id==LOOPA){if(!engine->seekable())throw std::runtime_error("A-B loop requires seekable media.");loop.start(engine->time());message(L"Loop A = "+clock(*loop.a)+L". Choose B later in this source.");return;}
  if(id==LOOPB){loop.end(engine->time());message(L"A-B loop enabled. Playback returns to A at B.");return;}
  if(id==LOOPCLEAR){loop.clear();message(L"A-B loop cleared.");return;}
  if(id==FRAME){engine->pause(true);if(treatment){closeComparison();message(L"Comparison closed: frame stepping is only exact within a single player.");}api->NextFrame(engine->player);return;}
  if(id==FXENABLE){effectsFromControls();return;}
  if(id==RESETFX){adjustments={};SendMessageW(GetDlgItem(hwnd,FXENABLE),BM_SETCHECK,BST_UNCHECKED,0);for(int i=0;i<4;++i)SendMessageW(GetDlgItem(hwnd,CONTRAST+i),TBM_SETPOS,TRUE,100);effectsFromControls();return;}
  if(id==FULLVLC){fullVlc();return;}
  if(id==SUBFILE){auto f=pick(hwnd);if(!f.empty()){engine->subtitle(f[0]);message(L"Subtitle added. Select its track in Subtitles.");}return;}
  if(id==AUDIODELAY||id==SUBDELAY){auto value=input(hwnd,L"Track timing",L"Delay in milliseconds, between -10000 and 10000. Positive delays the track.",L"0");if(value){size_t used=0;int n=std::stoi(*value,&used);if(used!=value->size()||n< -10000||n>10000)throw std::runtime_error("Invalid delay.");int rc=id==AUDIODELAY?api->SetAudioDelay(engine->player,static_cast<int64_t>(n)*1000):api->SetSubtitleDelay(engine->player,static_cast<int64_t>(n)*1000);if(rc<0)throw std::runtime_error("Track timing unavailable for this source.");if(id==AUDIODELAY)audioDelay=n;else subtitleDelay=n;}return;}
  if(id==SNAPSHOT){if(!api->HasVideo(engine->player))throw std::runtime_error("Play a video before taking a snapshot.");auto f=pick(hwnd,false,false,true);if(!f.empty()){if(!engine->snapshot(f[0]))throw std::runtime_error("This VLC output could not save a snapshot.");message(L"Displayed source frame saved. It may include enabled VLC filters/subtitles; it is not a raw source or DLSS result.");}return;}
  if(id==DIAGNOSTICS){auto f=pick(hwnd,false,false,true,L"json");if(!f.empty()){diagnostics(f[0]);message(L"Diagnostics saved without media paths or URLs.");}return;}
  if(id==COMPARE){if(!queue.selected||queue.items[*queue.selected].remote||!engine->seekable())throw std::runtime_error("Open a seekable local source first.");auto f=pick(hwnd);if(f.empty())return;
   if(MessageBoxW(hwnd,L"Choose the same video, timeline and crop processed by your external renderer. This is imported-output playback, not DLSS inference.\n\nThe source supplies the only audio. Synchronization is best-effort and not frame-exact. Continue?",L"Compare imported treatment",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;
   closeComparison();auto next=std::make_unique<Engine>(api,treatmentVideo,true);next->open({f[0].wstring(),L"Imported treatment",false});next->api->SetRate(next->player,playbackRate);treatment=std::move(next);engine->effects.enabled=false;engine->applyEffects();syncPending=true;lastSync=0;layout();message(L"Comparing imported treatment. Source audio only · best-effort sync · no model provenance verification.");return;}
  if(id==CLOSECOMPARE){closeComparison();message(L"Comparison closed.");return;}
  if(id==CHAPTERPREV||id==CHAPTERNEXT){closeComparison();if(id==CHAPTERPREV)api->PreviousChapter(engine->player);else api->NextChapter(engine->player);return;}
  if(id>=AUDIOFIRST&&id<AUDIOFIRST+999){auto i=static_cast<size_t>(id-AUDIOFIRST);if(i<audioTracks.size()&&api->SetAudioTrack(engine->player,audioTracks[i].first)<0)throw std::runtime_error("Could not select audio track.");return;}
  if(id>=SUBFIRST&&id<SUBFIRST+999){auto i=static_cast<size_t>(id-SUBFIRST);if(i<subTracks.size()&&api->SetSubtitle(engine->player,subTracks[i].first)<0)throw std::runtime_error("Could not select subtitle track.");return;}
  if(id>=EQFIRST&&id<EQFIRST+999){engine->equalizer(id-EQFIRST-1);eqIndex=id-EQFIRST-1;return;}
  if(id>=CHAPTERFIRST&&id<CHAPTERFIRST+999){closeComparison();api->SetChapter(engine->player,id-CHAPTERFIRST);return;}
  if(id>=ASPECTFIRST&&id<ASPECTFIRST+5){auto s=id==ASPECTFIRST?std::string{}:utf8(aspects[id-ASPECTFIRST]);api->SetAspect(engine->player,s.empty()?nullptr:s.c_str());if(treatment)api->SetAspect(treatment->player,s.empty()?nullptr:s.c_str());return;}
  if(id>=CROPFIRST&&id<CROPFIRST+5){auto s=id==CROPFIRST?std::string{}:utf8(aspects[id-CROPFIRST]);api->SetCrop(engine->player,s.empty()?nullptr:s.c_str());if(treatment)api->SetCrop(treatment->player,s.empty()?nullptr:s.c_str());return;}
  if(id>=DEINTFIRST&&id<DEINTFIRST+4){const char* modes[]={nullptr,"yadif","blend","bob"};api->SetDeinterlace(engine->player,modes[id-DEINTFIRST]);return;}
 }
 void tick(){
  if(engine){auto st=api->State(engine->player);SetWindowTextW(GetDlgItem(hwnd,PLAY),st==libvlc_Playing?L"Pause":L"Play");
   bool canSeek=engine->seekable();EnableWindow(seekBar,canSeek);if(!seeking){auto len=engine->length(),t=engine->time();SendMessageW(seekBar,TBM_SETPOS,TRUE,len>0?static_cast<LPARAM>(std::clamp<int64_t>(t*10000/len,0,10000)):0);SetWindowTextW(GetDlgItem(hwnd,303),(clock(t)+L" / "+clock(len)+(loop.b?L"   ·   A-B loop":L"")).c_str());}
   if(!fxApplied&&api->HasVideo(engine->player)){engine->effects=adjustments;engine->applyEffects();fxApplied=true;message(L"Playing with libVLC "+wide(api->runtimeVersion)+(engine->driverSuperResolutionRequested?L" · VSR REQUESTED, hardware unverified · not DLSS 5":L" · DLSS 5 inference unavailable"));}
   if(auto target=loop.target(engine->time(),engine->playing(),canSeek))seek(*target);
   if(treatment && GetTickCount64()-lastSync>500){lastSync=GetTickCount64();auto ts=api->State(treatment->player);
    if(ts==libvlc_Error||ts==libvlc_Ended){closeComparison();message(L"Imported comparison ended or could not decode; source playback is preserved.");}
    else if(ts==libvlc_Playing||ts==libvlc_Paused){
     if(engine->length()>0&&treatment->length()>0&&std::llabs(engine->length()-treatment->length())>1500){closeComparison();message(L"Comparison rejected: video durations differ by more than 1.5 seconds.");}
     else{if(syncPending||std::llabs(engine->time()-treatment->time())>120)treatment->seek(engine->time());treatment->pause(!engine->playing());syncPending=false;}
    }
   }
   if(st==libvlc_Ended&&!endHandled){endHandled=true;auto n=queue.next(true);if(n)select(*n);else message(L"Playback finished.");}
   if(st==libvlc_Error&&!endHandled){endHandled=true;closeComparison();message(L"VLC could not decode this source. Choose another file or use Full VLC for advanced setup.");}
  }
  if(smoke&&GetTickCount64()-started>5000){KillTimer(hwnd,1);try{diagnostics(smokeImage.parent_path()/L"ui-playback.json");if(!engine||!api->HasVideo(engine->player)||!saveWindow(hwnd,smokeImage))smokeExit=1;}catch(...){smokeExit=1;}PostMessageW(hwnd,WM_CLOSE,0,0);}
 }
 void drawButton(DRAWITEMSTRUCT* d){
  bool disabled=(d->itemState&ODS_DISABLED)!=0,pressed=(d->itemState&ODS_SELECTED)!=0;
  COLORREF color=pressed?RGB(46,71,80):panel;auto brush=CreateSolidBrush(color);FillRect(d->hDC,&d->rcItem,brush);DeleteObject(brush);
  auto pen=CreatePen(PS_SOLID,1,(d->itemState&ODS_FOCUS)?accent:RGB(56,70,86));auto oldPen=SelectObject(d->hDC,pen);auto oldBrush=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));Rectangle(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom);SelectObject(d->hDC,oldBrush);SelectObject(d->hDC,oldPen);DeleteObject(pen);
  wchar_t caption[128]{};GetWindowTextW(d->hwndItem,caption,128);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,disabled?muted:text);SelectObject(d->hDC,font);DrawTextW(d->hDC,caption,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 }
 bool key(MSG& msg){
  if(msg.message!=WM_KEYDOWN)return false;
  wchar_t cls[64]{};GetClassNameW(msg.hwnd,cls,64);if(std::wstring(cls)==L"Edit"||std::wstring(cls)==L"ComboBox"||std::wstring(cls)==TRACKBAR_CLASSW)return false;
  bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
  int id=0;if(ctrl&&msg.wParam=='O')id=OPEN;else if(ctrl&&msg.wParam=='N')id=NETWORK;else if(ctrl)return false;
  else switch(msg.wParam){case VK_SPACE:id=PLAY;break;case 'S':id=STOP;break;case 'E':id=FRAME;break;case 'F':id=FULLSCREEN;break;case 'M':id=MUTE;break;case 'N':id=NEXT;break;case 'P':id=PREV;break;case VK_OEM_4:id=LOOPA;break;case VK_OEM_6:id=LOOPB;break;
   case VK_ESCAPE:if(fullscreen)fullscreenToggle();else action(STOP);return true;
   case VK_LEFT:case VK_RIGHT:if(engine)seek(engine->time()+(msg.wParam==VK_LEFT?-1:1)*((GetKeyState(VK_SHIFT)&0x8000)?30000:5000));return true;}
  if(id){action(id);return true;}return false;
 }
 ~Window(){treatment.reset();engine.reset();api.reset();if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);}
};
LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto* p=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
 if(!p)return DefWindowProcW(h,m,w,l);
 try{switch(m){case WM_CREATE:p->create();return 0;case WM_SIZE:p->layout();return 0;
 case WM_GETMINMAXINFO:{auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={1000,690};return 0;}
 case WM_DPICHANGED:{auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);return 0;}
 case WM_INITMENUPOPUP:p->refreshMenu(reinterpret_cast<HMENU>(w));return 0;
 case WM_DRAWITEM:p->drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=reinterpret_cast<HDC>(w);SetTextColor(dc,text);SetBkColor(dc,m==WM_CTLCOLORSTATIC?bg:panel);return reinterpret_cast<LRESULT>(m==WM_CTLCOLORSTATIC?backgroundBrush:panelBrush);}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,text);SelectObject(dc,p->titleFont);RECT r{16,12,500,44};DrawTextW(dc,L"SHINY PLAYER",-1,&r,DT_SINGLELINE);SetTextColor(dc,muted);SelectObject(dc,p->font);r={218,18,900,44};DrawTextW(dc,L"Powered by libVLC  /  Windows desktop",-1,&r,DT_SINGLELINE);EndPaint(h,&ps);return 0;}
 case WM_COMMAND:{int id=LOWORD(w);if(id==QUEUE){if(HIWORD(w)==LBN_DBLCLK){auto i=SendMessageW(p->queueBox,LB_GETCURSEL,0,0);if(i!=LB_ERR)p->select(static_cast<size_t>(i));}}else if(id==RATE){if(HIWORD(w)==CBN_SELCHANGE&&p->engine){const float rates[]={.5f,.75f,1.f,1.25f,1.5f,2.f};auto i=SendMessageW(p->rateBox,CB_GETCURSEL,0,0);if(i>=0&&i<6){if(p->api->SetRate(p->engine->player,rates[i])<0)throw std::runtime_error("Playback speed unsupported for this input.");p->playbackRate=rates[i];if(p->treatment)p->api->SetRate(p->treatment->player,rates[i]);}}}else p->action(id);return 0;}
 case WM_HSCROLL:{auto control=reinterpret_cast<HWND>(l);if(control==p->seekBar){if(LOWORD(w)==TB_THUMBTRACK)p->seeking=true;else if(LOWORD(w)==TB_ENDTRACK||LOWORD(w)==TB_THUMBPOSITION||LOWORD(w)==TB_PAGEUP||LOWORD(w)==TB_PAGEDOWN||LOWORD(w)==TB_LINEUP||LOWORD(w)==TB_LINEDOWN){p->seeking=false;if(p->engine)p->seek(SendMessageW(p->seekBar,TBM_GETPOS,0,0)*p->engine->length()/10000);}}else if(control==p->volumeBar){p->volume=static_cast<int>(SendMessageW(p->volumeBar,TBM_GETPOS,0,0));if(p->engine)p->api->SetVolume(p->engine->player,p->volume);}else p->effectsFromControls();return 0;}
 case WM_DROPFILES:{auto drop=reinterpret_cast<HDROP>(w);std::vector<std::filesystem::path> files;UINT n=std::min<UINT>(1000,DragQueryFileW(drop,0xFFFFFFFF,nullptr,0));for(UINT i=0;i<n;++i){UINT len=DragQueryFileW(drop,i,nullptr,0);std::wstring s(static_cast<size_t>(len)+1,L'\0');DragQueryFileW(drop,i,s.data(),len+1);s.resize(len);files.emplace_back(s);}DragFinish(drop);p->addFiles(files);return 0;}
 case WM_TIMER:p->tick();return 0;case WM_CLOSE:KillTimer(h,1);p->treatment.reset();p->engine.reset();DestroyWindow(h);return 0;case WM_DESTROY:PostQuitMessage(p->smokeExit);return 0;
 }}catch(const std::exception& e){p->failure(e);if(p->smoke){p->smokeExit=1;PostMessageW(h,WM_CLOSE,0,0);}return 0;}
 return DefWindowProcW(h,m,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);int result=1;bool automated=false;for(int i=1;i<argc;++i)if(std::wstring(argv[i])==L"--self-test"||std::wstring(argv[i])==L"--ui-smoke")automated=true;
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Gdiplus::GdiplusStartupInput gdi;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&gdi,nullptr);
 try{
  if(argc==5&&std::wstring(argv[1])==L"--self-test"){result=playbackTest(argv[2],argv[3],argv[4]);}
  else{
   INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
   backgroundBrush=CreateSolidBrush(bg);panelBrush=CreateSolidBrush(panel);
   WNDCLASSW wc{};wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=backgroundBrush;wc.lpfnWndProc=windowProc;wc.lpszClassName=L"ShinyVlcPlayer";RegisterClassW(&wc);
   wc.lpfnWndProc=inputProc;wc.lpszClassName=L"ShinyPlayerInput";wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
   Window window;std::vector<std::filesystem::path> files;
   for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--vlc-dir"&&i+1<argc)window.runtimeFolder=argv[++i];else if(arg==L"--ui-smoke"&&i+2<argc){window.smoke=true;files.emplace_back(argv[++i]);window.smokeImage=argv[++i];}else if(arg.starts_with(L"--"))throw std::runtime_error("Unknown player option.");else files.emplace_back(arg);}
   HWND h=CreateWindowExW(0,L"ShinyVlcPlayer",L"Shiny Player",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,nullptr,nullptr,instance,&window);
   if(!h)throw std::runtime_error("Cannot create player window.");ShowWindow(h,show);UpdateWindow(h);if(!files.empty())window.addFiles(files);
   MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){try{if(window.key(msg))continue;}catch(const std::exception&e){window.failure(e);}TranslateMessage(&msg);DispatchMessageW(&msg);}result=static_cast<int>(msg.wParam);
  }
 }catch(const std::exception& e){if(!automated)MessageBoxW(nullptr,wide(e.what()).c_str(),L"Shiny Player",MB_OK|MB_ICONERROR);}
 if(backgroundBrush)DeleteObject(backgroundBrush);if(panelBrush)DeleteObject(panelBrush);Gdiplus::GdiplusShutdown(token);CoUninitialize();LocalFree(argv);return result;
}
