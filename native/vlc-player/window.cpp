// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include <uxtheme.h>
namespace shiny::ui {
HWND Window::control(const wchar_t* cls,const wchar_t* name,int id,DWORD extra){
 HWND h=CreateWindowExW(0,cls,name,WS_CHILD|WS_VISIBLE|WS_TABSTOP|extra,0,0,10,10,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
 SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);controls.push_back(h);return h;
}
HWND Window::button(const wchar_t* name,int id){auto h=control(L"BUTTON",name,id,BS_OWNERDRAW);SetWindowSubclass(h,design::hoverProc,1,0);return h;}
HWND Window::label(const wchar_t* name,int id){auto h=control(L"STATIC",name,id,SS_LEFT|SS_NOPREFIX);SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)&~WS_TABSTOP);return h;}
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
 add(tools,COMMANDS,L"Quick actions...\tCtrl+K");add(tools,SHOWQUEUE,L"Queue workspace\tCtrl+F");add(tools,SHOWFX,L"Live adjustments");add(tools,LIBRARIES,L"Enhancement libraries...");add(tools,MANAGEDPREVIEW,L"Managed spatial preview (not DLSS)...");add(tools,NR,L"Native Neural Workbench...");add(tools,MEDIAINFO,L"Media information");add(tools,COMPARE,L"Compare an externally processed video...");add(tools,CLOSECOMPARE,L"Close comparison");add(tools,DLSS,L"DLSS 5 integration status");add(tools,DIAGNOSTICS,L"Save playback diagnostics...");add(tools,ABOUT,L"About / keyboard shortcuts");
 for(auto entry:{std::pair{media,L"Media"},std::pair{playback,L"Playback"},std::pair{audio,L"Audio"},std::pair{v,L"Video"},std::pair{sub,L"Subtitles"},std::pair{tools,L"Tools"}})AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(entry.first),entry.second);
 SetMenu(hwnd,menu);
}
void Window::applyDpi(UINT value){
 theme.set(hwnd,value);dpi=theme.dpi;font=theme.body;titleFont=theme.heading;
 if(queueBox)SendMessageW(queueBox,LB_SETITEMHEIGHT,0,units(60));
 if(auto title=GetDlgItem(hwnd,WELCOMETITLE))SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
}
void Window::create(){
 applyDpi(GetDpiForWindow(hwnd));
 createMenus();button(L"Quick actions  Ctrl+K",COMMANDS);label(L"READY",STATEBADGE);button(L"Open media",OPEN);button(L"Network stream",NETWORK);button(L"Video studio",MANAGEDPREVIEW);button(L"Libraries",LIBRARIES);button(L"Full screen",FULLSCREEN);button(L"Cinema view",CINEMA);button(L"DLSS-NR research",NR);
 primaryLabel=label(L"SOURCE / VLC LIVE ADJUSTMENTS",300);secondaryLabel=label(L"IMPORTED TREATMENT - PROVENANCE UNVERIFIED",301);
 video=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
 treatmentVideo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
 label(L"PLAY QUEUE",302);queueBox=control(L"LISTBOX",L"Play queue",QUEUE,LBS_NOTIFY|WS_VSCROLL|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS);
 button(L"Clear",CLEAR);button(L"Up",QUEUEUP);button(L"Down",QUEUEDOWN);button(L"Remove",REMOVE);
 button(L"Queue",SHOWQUEUE);button(L"Adjustments",SHOWFX);
 auto search=control(L"EDIT",L"",QUEUESEARCH,ES_AUTOHSCROLL|WS_BORDER);SendMessageW(search,EM_SETLIMITTEXT,256,0);
 SendMessageW(search,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Filter titles…"));label(L"",QUEUECOUNT);
 auto welcome=label(L"",WELCOMETITLE);SetWindowLongPtrW(welcome,GWL_STYLE,(GetWindowLongPtrW(welcome,GWL_STYLE)&~SS_TYPEMASK)|SS_CENTER);
 auto body=label(L"",WELCOMEBODY);SetWindowLongPtrW(body,GWL_STYLE,(GetWindowLongPtrW(body,GWL_STYLE)&~SS_TYPEMASK)|SS_CENTER);
 button(L"Open media",WELCOMEOPEN);button(L"Network stream",WELCOMENET);
 auto effectsToggle=control(L"BUTTON",L"Live VLC adjustments (not DLSS)",FXENABLE,BS_AUTOCHECKBOX);SetWindowTheme(effectsToggle,L"",L"");
 const int ids[]={CONTRAST,BRIGHTNESS,SATURATION,GAMMA};const wchar_t* names[]={L"Contrast",L"Brightness",L"Saturation",L"Gamma"};
 for(int i=0;i<4;++i){label(names[i],310+i);auto s=control(TRACKBAR_CLASSW,names[i],ids[i],TBS_HORZ|TBS_NOTICKS);SendMessageW(s,TBM_SETRANGE,TRUE,MAKELPARAM(10,300));SendMessageW(s,TBM_SETPOS,TRUE,100);}
 button(L"Reset adjustments",RESETFX);button(L"Model / enhancement status",DLSS);
 label(L"00:00 / --:--",303);seekBar=control(TRACKBAR_CLASSW,L"Playback position",SEEK,TBS_HORZ|TBS_NOTICKS);SendMessageW(seekBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));
 button(L"Play",PLAY);button(L"Stop",STOP);button(L"Previous",PREV);button(L"Next",NEXT);button(L"Save frame",SNAPSHOT);
 rateBox=control(L"COMBOBOX",L"",RATE,CBS_DROPDOWNLIST);for(auto s:{L"0.5x",L"0.75x",L"1x",L"1.25x",L"1.5x",L"2x"})SendMessageW(rateBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(rateBox,CB_SETCURSEL,2,0);
 button(L"A",LOOPA);button(L"B",LOOPB);button(L"Clear A-B",LOOPCLEAR);button(L"Mute",MUTE);
 volumeBar=control(TRACKBAR_CLASSW,L"Volume",VOLUME,TBS_HORZ|TBS_NOTICKS);SendMessageW(volumeBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(volumeBar,TBM_SETPOS,TRUE,volume);
 applyDpi(dpi);refreshQueue();status=label(L"Loading installed VLC...",304);BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));DragAcceptFiles(hwnd,TRUE);SetTimer(hwnd,1,150,nullptr);
 try{loadRuntime(runtimeFolder.empty()?installedVlc():runtimeFolder);}catch(const std::exception& e){message(wide(e.what())+L"  Media > Locate installed VLC.");}
 tips=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,hwnd,nullptr,GetModuleHandleW(nullptr),nullptr);
 SendMessageW(tips,TTM_SETMAXTIPWIDTH,0,360);
 for(auto pair:{std::pair{OPEN,L"Open local files (Ctrl+O); drag and drop is also supported."},std::pair{STOP,L"Stop playback and close independent previews (S or Escape)."},std::pair{COMMANDS,L"Search existing commands (Ctrl+K). Nothing runs while typing."},std::pair{MANAGEDPREVIEW,L"A separate muted SDR comparison. Play a local video first."},std::pair{LIBRARIES,L"Import, verify and stage packages; selection applies to the next preview."},std::pair{LOOPA,L"Set the beginning of an A–B loop ([)."},std::pair{LOOPB,L"Set the end of an A–B loop (])."},std::pair{QUEUESEARCH,L"Search displayed titles only. Paths and stream URLs are never searched."}}){
  TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tool.hwnd=hwnd;tool.uId=reinterpret_cast<UINT_PTR>(GetDlgItem(hwnd,pair.first));tool.lpszText=const_cast<wchar_t*>(pair.second);SendMessageW(tips,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));
 }
 started=GetTickCount64();updateWorkspace();layout();
}
void Window::layout(){
 RECT r{};GetClientRect(hwnd,&r);const int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);
 auto a=workspaceLayout(w,h,cinema,treatment!=nullptr);
 auto pos=[&](int id,Box b){move(id,b.x,b.y,b.w,b.h);};
 const int toolbarIds[]={OPEN,NETWORK,MANAGEDPREVIEW,LIBRARIES,NR,CINEMA,FULLSCREEN};
 for(size_t i=0;i<a.toolbar.size();++i)pos(toolbarIds[i],a.toolbar[i]);
 auto videoPos=[&](HWND child,Box b){MoveWindow(child,units(b.x),units(b.y),units(std::max(1,b.w)),units(b.h),TRUE);};
 move(300,a.original.x,a.original.y-26,a.original.w,20);videoPos(video,a.original);
 ShowWindow(treatmentVideo,treatment?SW_SHOW:SW_HIDE);ShowWindow(secondaryLabel,treatment?SW_SHOW:SW_HIDE);
 if(treatment){move(301,a.comparison.x,a.comparison.y-26,a.comparison.w,20);videoPos(treatmentVideo,a.comparison);}
 // Recover keyboard focus before hiding a sidebar (resize/cinema/tab switch).
 const int queueControls[]={QUEUE,QUEUESEARCH,QUEUECOUNT,QUEUEUP,QUEUEDOWN,REMOVE,CLEAR};
 const int adjustmentControls[]={FXENABLE,310,311,312,313,CONTRAST,BRIGHTNESS,SATURATION,GAMMA,RESETFX,DLSS};
 auto visibility=[&](int id,bool show){auto child=GetDlgItem(hwnd,id);if(!show&&GetFocus()==child)SetFocus(GetDlgItem(hwnd,COMMANDS));ShowWindow(child,show?SW_SHOW:SW_HIDE);};
 visibility(302,false);for(int id:{SHOWQUEUE,SHOWFX})visibility(id,a.showSidebar);
 for(int id:queueControls)visibility(id,a.showSidebar&&!showAdjustments);
 for(int id:adjustmentControls)visibility(id,a.showSidebar&&showAdjustments);
 if(a.showSidebar){
  auto b=a.sidebar;move(SHOWQUEUE,b.x,b.y-31,128,28);move(SHOWFX,b.x+138,b.y-31,130,28);
  if(!showAdjustments){
   move(QUEUESEARCH,b.x,b.y,268,30);move(QUEUE,b.x,b.y+42,268,b.h-126);
   move(QUEUECOUNT,b.x,b.y+b.h-77,268,34);
   move(QUEUEUP,b.x,b.y+b.h-35,54,32);move(QUEUEDOWN,b.x+60,b.y+b.h-35,58,32);move(REMOVE,b.x+124,b.y+b.h-35,76,32);move(CLEAR,b.x+206,b.y+b.h-35,62,32);
  }else{
   move(FXENABLE,b.x,b.y,268,28);int y=b.y+44;
   for(int i=0;i<4;++i){move(310+i,b.x,y,268,18);move(CONTRAST+i,b.x,y+20,268,24);y+=50;}
   move(RESETFX,b.x,b.y+b.h-36,268,32);move(DLSS,b.x,b.y+b.h-73,268,28);
  }
 }
 move(COMMANDS,w-188,20,168,38);move(STATEBADGE,w-326,31,116,22);
 ShowWindow(video,workspaceEmpty?SW_HIDE:SW_SHOW);
 for(int id:{WELCOMETITLE,WELCOMEBODY,WELCOMEOPEN,WELCOMENET})visibility(id,workspaceEmpty);
 if(workspaceEmpty){auto b=a.original;const bool roomy=b.h>=240;int y=roomy?b.y+b.h/2-70:b.y+14;
  move(WELCOMETITLE,b.x+16,y,b.w-32,34);ShowWindow(GetDlgItem(hwnd,WELCOMEBODY),roomy?SW_SHOW:SW_HIDE);
  if(roomy)move(WELCOMEBODY,b.x+24,y+46,b.w-48,48);
  const int buttonsY=roomy?y+116:y+48;move(WELCOMEOPEN,b.x+b.w/2-155,buttonsY,148,36);move(WELCOMENET,b.x+b.w/2+7,buttonsY,148,36);
 }
 const int transportIds[]={PLAY,STOP,PREV,NEXT,SNAPSHOT,RATE,LOOPA,LOOPB,LOOPCLEAR,MUTE,VOLUME};
 for(size_t i=0;i<a.transport.size();++i){auto b=a.transport[i];if(transportIds[i]==RATE)b.h=180;pos(transportIds[i],b);}
 pos(303,a.time);pos(SEEK,a.seek);pos(304,a.status);InvalidateRect(hwnd,nullptr,TRUE);
}
void Window::drawButton(DRAWITEMSTRUCT* d){
 if(d->CtlID==QUEUE){drawQueue(*d);return;}
 const bool primary=d->CtlID==OPEN||d->CtlID==PLAY||d->CtlID==WELCOMEOPEN||
  (d->CtlID==SHOWQUEUE&&!showAdjustments)||(d->CtlID==SHOWFX&&showAdjustments);
 theme.button(*d,primary);
}
}
