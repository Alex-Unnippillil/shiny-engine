// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include <uxtheme.h>
namespace shiny::ui {
HWND Window::control(const wchar_t* cls,const wchar_t* name,int id,DWORD extra){
 HWND h=CreateWindowExW(0,cls,name,WS_CHILD|WS_VISIBLE|WS_TABSTOP|extra,0,0,10,10,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
 SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);controls.push_back(h);return h;
}
HWND Window::button(const wchar_t* name,int id){return control(L"BUTTON",name,id,BS_OWNERDRAW);}
HWND Window::label(const wchar_t* name,int id){auto h=control(L"STATIC",name,id,SS_LEFT);SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)&~WS_TABSTOP);return h;}
void Window::move(int id,int x,int y,int w,int h){if(auto c=GetDlgItem(hwnd,id))MoveWindow(c,units(x),units(y),units(std::max(1,w)),units(std::max(1,h)),TRUE);}
void Window::message(const std::wstring& value){SetWindowTextW(status,value.c_str());}
void Window::failure(const std::exception& e){message(wide(e.what()));if(!smoke)MessageBoxW(hwnd,wide(e.what()).c_str(),L"Shiny Player",MB_OK|MB_ICONWARNING);}
void Window::createMenus(){
 menu=CreateMenu();auto media=CreatePopupMenu(),playback=CreatePopupMenu(),audio=CreatePopupMenu(),v=CreatePopupMenu(),sub=CreatePopupMenu(),tools=CreatePopupMenu();
 auto add=[](HMENU m,int id,const wchar_t* s){AppendMenuW(m,MF_STRING,static_cast<UINT_PTR>(id),s);};
 add(media,OPEN,L"Open files...\tCtrl+O");add(media,NETWORK,L"Open network stream...\tCtrl+N");add(media,LOCATE,L"Locate installed VLC...");
 add(media,QUEUEUP,L"Move queue item up");add(media,QUEUEDOWN,L"Move queue item down");add(media,REMOVE,L"Remove selected queue item");add(media,CLEAR,L"Clear queue and stop");add(media,SAVEQUEUE,L"Export queue as M3U8...");add(media,FULLVLC,L"Open full VLC interface");
 add(playback,JUMP,L"Go to time...\tJ");add(playback,BOOKMARK,L"Bookmark current time\tB");bookmarkMenu=CreatePopupMenu();AppendMenuW(playback,MF_POPUP,reinterpret_cast<UINT_PTR>(bookmarkMenu),L"This source bookmarks");
 queueMenu=CreatePopupMenu();AppendMenuW(playback,MF_POPUP,reinterpret_cast<UINT_PTR>(queueMenu),L"Choose queued item");
 add(playback,PLAY,L"Play / Pause\tSpace");add(playback,STOP,L"Stop\tS");add(playback,PREV,L"Previous item\tP");add(playback,NEXT,L"Next item\tN");add(playback,FRAME,L"Next frame\tE");
 add(playback,REPEAT,L"Repeat current item");add(playback,SHUFFLE,L"Shuffle queue");add(playback,LOOPA,L"Set loop A\t[");add(playback,LOOPB,L"Set loop B\t]");add(playback,LOOPCLEAR,L"Clear A-B loop");
 chapterMenu=CreatePopupMenu();AppendMenuW(playback,MF_POPUP,reinterpret_cast<UINT_PTR>(chapterMenu),L"Chapters");add(playback,CHAPTERPREV,L"Previous chapter");add(playback,CHAPTERNEXT,L"Next chapter");
 deviceMenu=CreatePopupMenu();AppendMenuW(audio,MF_POPUP,reinterpret_cast<UINT_PTR>(deviceMenu),L"Output device");add(audio,MUTE,L"Mute\tM");audioMenu=CreatePopupMenu();AppendMenuW(audio,MF_POPUP,reinterpret_cast<UINT_PTR>(audioMenu),L"Audio track");
 eqMenu=CreatePopupMenu();AppendMenuW(audio,MF_POPUP,reinterpret_cast<UINT_PTR>(eqMenu),L"Equalizer presets");add(audio,AUDIODELAY,L"Audio delay (milliseconds)...");
 auto aspect=CreatePopupMenu(),crop=CreatePopupMenu(),deint=CreatePopupMenu();for(int i=0;i<5;++i){add(aspect,ASPECTFIRST+i,aspects[i]);add(crop,CROPFIRST+i,aspects[i]);}
 for(int i=0;i<4;++i)add(deint,DEINTFIRST+i,deinterlace[i]);AppendMenuW(v,MF_POPUP,reinterpret_cast<UINT_PTR>(aspect),L"Aspect ratio");AppendMenuW(v,MF_POPUP,reinterpret_cast<UINT_PTR>(crop),L"Crop");AppendMenuW(v,MF_POPUP,reinterpret_cast<UINT_PTR>(deint),L"Deinterlace");
 add(v,CINEMA,L"Toggle cinema view\tC");add(v,TOPMOST,L"Always on top");add(v,VSR,L"Request driver super resolution on next open (not DLSS)");add(v,FULLSCREEN,L"Full screen\tF");add(v,SNAPSHOT,L"Save displayed frame PNG...");
 add(sub,SUBFILE,L"Load subtitle file...");subMenu=CreatePopupMenu();AppendMenuW(sub,MF_POPUP,reinterpret_cast<UINT_PTR>(subMenu),L"Subtitle track");add(sub,SUBDELAY,L"Subtitle delay (milliseconds)...");
 add(tools,LIBRARIES,L"Enhancement libraries...");add(tools,MANAGEDPREVIEW,L"Managed spatial preview (not DLSS)...");add(tools,NR,L"Native Neural Workbench...");add(tools,MEDIAINFO,L"Media information");add(tools,COMPARE,L"Compare an externally processed video...");add(tools,CLOSECOMPARE,L"Close comparison");add(tools,DLSS,L"DLSS 5 integration status");add(tools,DIAGNOSTICS,L"Save playback diagnostics...");add(tools,ABOUT,L"About / keyboard shortcuts");
 for(auto entry:{std::pair{media,L"Media"},std::pair{playback,L"Playback"},std::pair{audio,L"Audio"},std::pair{v,L"Video"},std::pair{sub,L"Subtitles"},std::pair{tools,L"Tools"}})AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(entry.first),entry.second);
 SetMenu(hwnd,menu);
}
void Window::applyDpi(UINT value){
 dpi=value?value:96;auto old=font,oldTitle=titleFont;
 font=CreateFontW(-units(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 titleFont=CreateFontW(-units(24),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 for(auto child:controls)SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
 if(old)DeleteObject(old);if(oldTitle)DeleteObject(oldTitle);
}
void Window::create(){
 applyDpi(GetDpiForWindow(hwnd));
 createMenus();button(L"Open media",OPEN);button(L"Network stream",NETWORK);button(L"Compare video",COMPARE);button(L"Full VLC",FULLVLC);button(L"Full screen",FULLSCREEN);button(L"Cinema view",CINEMA);button(L"DLSS-NR research",NR);
 primaryLabel=label(L"SOURCE / VLC LIVE ADJUSTMENTS",300);secondaryLabel=label(L"IMPORTED TREATMENT - PROVENANCE UNVERIFIED",301);
 video=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
 treatmentVideo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
 label(L"PLAY QUEUE",302);queueBox=control(L"LISTBOX",L"",QUEUE,LBS_NOTIFY|WS_VSCROLL|LBS_NOINTEGRALHEIGHT);button(L"Clear",CLEAR);
 auto effectsToggle=control(L"BUTTON",L"Live VLC adjustments (not DLSS)",FXENABLE,BS_AUTOCHECKBOX);SetWindowTheme(effectsToggle,L"",L"");
 const int ids[]={CONTRAST,BRIGHTNESS,SATURATION,GAMMA};const wchar_t* names[]={L"Contrast",L"Brightness",L"Saturation",L"Gamma"};
 for(int i=0;i<4;++i){label(names[i],310+i);auto s=control(TRACKBAR_CLASSW,names[i],ids[i],TBS_HORZ|TBS_NOTICKS);SendMessageW(s,TBM_SETRANGE,TRUE,MAKELPARAM(10,300));SendMessageW(s,TBM_SETPOS,TRUE,100);}
 button(L"Reset adjustments",RESETFX);button(L"Model / enhancement status",DLSS);
 label(L"00:00 / --:--",303);seekBar=control(TRACKBAR_CLASSW,L"Playback position",SEEK,TBS_HORZ|TBS_NOTICKS);SendMessageW(seekBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));
 button(L"Play",PLAY);button(L"Stop",STOP);button(L"Previous",PREV);button(L"Next",NEXT);button(L"Save frame",SNAPSHOT);
 rateBox=control(L"COMBOBOX",L"",RATE,CBS_DROPDOWNLIST);for(auto s:{L"0.5x",L"0.75x",L"1x",L"1.25x",L"1.5x",L"2x"})SendMessageW(rateBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(rateBox,CB_SETCURSEL,2,0);
 button(L"A",LOOPA);button(L"B",LOOPB);button(L"Clear A-B",LOOPCLEAR);button(L"Mute",MUTE);
 volumeBar=control(TRACKBAR_CLASSW,L"Volume",VOLUME,TBS_HORZ|TBS_NOTICKS);SendMessageW(volumeBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(volumeBar,TBM_SETPOS,TRUE,volume);
 status=label(L"Loading installed VLC...",304);BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));DragAcceptFiles(hwnd,TRUE);SetTimer(hwnd,1,150,nullptr);
 try{loadRuntime(runtimeFolder.empty()?installedVlc():runtimeFolder);}catch(const std::exception& e){message(wide(e.what())+L"  Media > Locate installed VLC.");}
 started=GetTickCount64();layout();
}
void Window::layout(){
 RECT r{};GetClientRect(hwnd,&r);const int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);
 auto a=workspaceLayout(w,h,cinema,treatment!=nullptr);
 auto pos=[&](int id,Box b){move(id,b.x,b.y,b.w,b.h);};
 const int toolbarIds[]={OPEN,NETWORK,COMPARE,FULLVLC,NR,CINEMA,FULLSCREEN};
 for(size_t i=0;i<a.toolbar.size();++i)pos(toolbarIds[i],a.toolbar[i]);
 auto videoPos=[&](HWND child,Box b){MoveWindow(child,units(b.x),units(b.y),units(std::max(1,b.w)),units(b.h),TRUE);};
 move(300,a.original.x,a.original.y-26,a.original.w,20);videoPos(video,a.original);
 ShowWindow(treatmentVideo,treatment?SW_SHOW:SW_HIDE);ShowWindow(secondaryLabel,treatment?SW_SHOW:SW_HIDE);
 if(treatment){move(301,a.comparison.x,a.comparison.y-26,a.comparison.w,20);videoPos(treatmentVideo,a.comparison);}
 for(int id:{302,CLEAR,QUEUE,FXENABLE,310,311,312,313,CONTRAST,BRIGHTNESS,SATURATION,GAMMA,RESETFX,DLSS})ShowWindow(GetDlgItem(hwnd,id),a.showSidebar?SW_SHOW:SW_HIDE);
 if(a.showSidebar){
  auto b=a.sidebar;move(302,b.x,b.y-26,160,24);move(CLEAR,b.x+190,b.y-31,78,26);
  int listH=std::max(65,b.h-266);move(QUEUE,b.x,b.y,268,listH);int y=b.y+listH+9;
  move(FXENABLE,b.x,y,268,24);y+=30;
  for(int i=0;i<4;++i){move(310+i,b.x,y+3,88,24);move(CONTRAST+i,b.x+86,y,182,26);y+=32;}
  move(RESETFX,b.x,y+2,268,28);move(DLSS,b.x,y+38,268,28);
 }
 const int transportIds[]={PLAY,STOP,PREV,NEXT,SNAPSHOT,RATE,LOOPA,LOOPB,LOOPCLEAR,MUTE,VOLUME};
 for(size_t i=0;i<a.transport.size();++i){auto b=a.transport[i];if(transportIds[i]==RATE)b.h=180;pos(transportIds[i],b);}
 pos(303,a.time);pos(SEEK,a.seek);pos(304,a.status);InvalidateRect(hwnd,nullptr,TRUE);
}
void Window::drawButton(DRAWITEMSTRUCT* d){
 const bool disabled=(d->itemState&ODS_DISABLED)!=0,pressed=(d->itemState&ODS_SELECTED)!=0,primary=(d->CtlID==OPEN||d->CtlID==PLAY);
 FillRect(d->hDC,&d->rcItem,backgroundBrush);auto brush=CreateSolidBrush(primary&&!disabled?(pressed?RGB(70,188,161):accent):(pressed?RGB(46,71,80):panel));
 auto pen=CreatePen(PS_SOLID,1,(d->itemState&ODS_FOCUS)?RGB(204,255,242):(primary?accent:RGB(56,70,86)));auto oldPen=SelectObject(d->hDC,pen);auto oldBrush=SelectObject(d->hDC,brush);RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,units(10),units(10));SelectObject(d->hDC,oldBrush);SelectObject(d->hDC,oldPen);DeleteObject(pen);DeleteObject(brush);
 wchar_t caption[128]{};GetWindowTextW(d->hwndItem,caption,128);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,disabled?muted:primary?bg:text);auto oldFont=SelectObject(d->hDC,font);DrawTextW(d->hDC,caption,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(d->hDC,oldFont);
}
}
