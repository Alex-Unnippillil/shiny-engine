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

void Window::move(int id,int x,int y,int w,int h){if(auto c=GetDlgItem(hwnd,id))MoveWindow(c,x,y,std::max(1,w),std::max(1,h),TRUE);}

void Window::message(const std::wstring& value){SetWindowTextW(status,value.c_str());}

void Window::failure(const std::exception& e){message(wide(e.what()));if(!smoke)MessageBoxW(hwnd,wide(e.what()).c_str(),L"Shiny Player",MB_OK|MB_ICONWARNING);}

void Window::createMenus(){
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

void Window::create(){
  font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  titleFont=CreateFontW(-24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  createMenus();button(L"Open media",OPEN);button(L"Network stream",NETWORK);button(L"Compare video",COMPARE);button(L"Full VLC",FULLVLC);button(L"Full screen",FULLSCREEN);
  primaryLabel=label(L"SOURCE / VLC LIVE ADJUSTMENTS",300);secondaryLabel=label(L"IMPORTED TREATMENT - PROVENANCE UNVERIFIED",301);
  video=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
  treatmentVideo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_CLIPSIBLINGS|SS_BLACKRECT,0,0,100,100,hwnd,nullptr,nullptr,nullptr);
  label(L"PLAY QUEUE",302);queueBox=control(L"LISTBOX",L"",QUEUE,LBS_NOTIFY|WS_VSCROLL|LBS_NOINTEGRALHEIGHT);button(L"Clear",CLEAR);
  auto effectsToggle=control(L"BUTTON",L"Live VLC adjustments (not DLSS)",FXENABLE,BS_AUTOCHECKBOX);
  SetWindowTheme(effectsToggle,L"",L""); // Use explicit dark text colors, not the default themed black caption.
  const int ids[]={CONTRAST,BRIGHTNESS,SATURATION,GAMMA};const wchar_t* names[]={L"Contrast",L"Brightness",L"Saturation",L"Gamma"};
  for(int i=0;i<4;++i){label(names[i],310+i);auto s=control(TRACKBAR_CLASSW,names[i],ids[i],TBS_HORZ|TBS_NOTICKS);SendMessageW(s,TBM_SETRANGE,TRUE,MAKELPARAM(10,300));SendMessageW(s,TBM_SETPOS,TRUE,100);}
  button(L"Reset adjustments",RESETFX);button(L"DLSS 5 status",DLSS);
  label(L"00:00 / --:--",303);seekBar=control(TRACKBAR_CLASSW,L"Playback position",SEEK,TBS_HORZ|TBS_NOTICKS);SendMessageW(seekBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));
  button(L"Play",PLAY);button(L"Stop",STOP);button(L"Previous",PREV);button(L"Next",NEXT);button(L"Save frame",SNAPSHOT);
  rateBox=control(L"COMBOBOX",L"",RATE,CBS_DROPDOWNLIST);for(auto s:{L"0.5x",L"0.75x",L"1x",L"1.25x",L"1.5x",L"2x"})SendMessageW(rateBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(rateBox,CB_SETCURSEL,2,0);
  button(L"A",LOOPA);button(L"B",LOOPB);button(L"Clear A-B",LOOPCLEAR);button(L"Mute",MUTE);
  volumeBar=control(TRACKBAR_CLASSW,L"Volume",VOLUME,TBS_HORZ|TBS_NOTICKS);SendMessageW(volumeBar,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(volumeBar,TBM_SETPOS,TRUE,volume);
  status=label(L"Loading installed VLC...",304);
  BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));DragAcceptFiles(hwnd,TRUE);SetTimer(hwnd,1,150,nullptr);
  try{loadRuntime(runtimeFolder.empty()?installedVlc():runtimeFolder);}catch(const std::exception& e){message(wide(e.what())+L"  Media > Locate installed VLC.");}
  started=GetTickCount64();layout();
 }

void Window::layout(){
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

void Window::drawButton(DRAWITEMSTRUCT* d){
  bool disabled=(d->itemState&ODS_DISABLED)!=0,pressed=(d->itemState&ODS_SELECTED)!=0;
  COLORREF color=pressed?RGB(46,71,80):panel;auto brush=CreateSolidBrush(color);FillRect(d->hDC,&d->rcItem,brush);DeleteObject(brush);
  auto pen=CreatePen(PS_SOLID,1,(d->itemState&ODS_FOCUS)?accent:RGB(56,70,86));auto oldPen=SelectObject(d->hDC,pen);auto oldBrush=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));Rectangle(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom);SelectObject(d->hDC,oldBrush);SelectObject(d->hDC,oldPen);DeleteObject(pen);
  wchar_t caption[128]{};GetWindowTextW(d->hwndItem,caption,128);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,disabled?muted:text);SelectObject(d->hDC,font);DrawTextW(d->hDC,caption,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 }
}
