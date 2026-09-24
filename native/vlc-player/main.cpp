// SPDX-License-Identifier: MIT
#include "ui.hpp"
namespace shiny::ui {
LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto* p=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){p=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}if(!p)return DefWindowProcW(h,m,w,l);
 try{switch(m){case WM_CREATE:p->create();return 0;case WM_SIZE:p->layout();return 0;
 case WM_GETMINMAXINFO:{RECT bounds{0,0,p->units(640),p->units(500)};AdjustWindowRectExForDpi(&bounds,WS_OVERLAPPEDWINDOW,TRUE,0,p->dpi);reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={bounds.right-bounds.left,bounds.bottom-bounds.top};return 0;}
 case WM_DPICHANGED:{p->applyDpi(HIWORD(w));auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);return 0;}
 case WM_INITMENUPOPUP:p->refreshMenu(reinterpret_cast<HMENU>(w));return 0;
 case WM_DRAWITEM:p->drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
 case WM_CTLCOLORBTN:case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=reinterpret_cast<HDC>(w);SetTextColor(dc,text);SetBkColor(dc,(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN)?bg:panel);return reinterpret_cast<LRESULT>((m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN)?backgroundBrush:panelBrush);}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,text);auto old=SelectObject(dc,p->titleFont);RECT r{p->units(20),p->units(18),p->units(215),p->units(50)};DrawTextW(dc,L"SHINY PLAYER",-1,&r,DT_SINGLELINE);SetTextColor(dc,muted);SelectObject(dc,p->font);RECT client{};GetClientRect(h,&client);r={p->units(225),p->units(25),client.right-p->units(20),p->units(50)};DrawTextW(dc,L"0.9  /  LOCAL PLAYBACK · VIDEO STUDIO",-1,&r,DT_SINGLELINE|DT_END_ELLIPSIS);SelectObject(dc,old);EndPaint(h,&ps);return 0;}
 case WM_COMMAND:{int id=LOWORD(w);if(id==QUEUE){if(HIWORD(w)==LBN_DBLCLK){auto i=SendMessageW(p->queueBox,LB_GETCURSEL,0,0);if(i!=LB_ERR)p->select(static_cast<size_t>(i));}}else if(id==RATE){if(HIWORD(w)==CBN_SELCHANGE&&p->engine){const float rates[]={.5f,.75f,1.f,1.25f,1.5f,2.f};auto i=SendMessageW(p->rateBox,CB_GETCURSEL,0,0);if(i>=0&&i<6){if(p->api->SetRate(p->engine->player,rates[i])<0)throw std::runtime_error("Playback speed unsupported for this input.");p->playbackRate=rates[i];if(p->treatment)p->api->SetRate(p->treatment->player,rates[i]);}}}else p->action(id);return 0;}
 case WM_HSCROLL:{auto c=reinterpret_cast<HWND>(l);if(c==p->seekBar){if(LOWORD(w)==TB_THUMBTRACK)p->seeking=true;else if(LOWORD(w)==TB_ENDTRACK||LOWORD(w)==TB_THUMBPOSITION||LOWORD(w)==TB_PAGEUP||LOWORD(w)==TB_PAGEDOWN||LOWORD(w)==TB_LINEUP||LOWORD(w)==TB_LINEDOWN){p->seeking=false;if(p->engine)p->seek(SendMessageW(p->seekBar,TBM_GETPOS,0,0)*p->engine->length()/10000);}}else if(c==p->volumeBar){p->volume=static_cast<int>(SendMessageW(p->volumeBar,TBM_GETPOS,0,0));if(p->engine)p->api->SetVolume(p->engine->player,p->volume);}else p->effectsFromControls();return 0;}
 case WM_DROPFILES:{auto drop=reinterpret_cast<HDROP>(w);std::vector<std::filesystem::path> files;UINT n=std::min<UINT>(1000,DragQueryFileW(drop,0xFFFFFFFF,nullptr,0));for(UINT i=0;i<n;++i){UINT len=DragQueryFileW(drop,i,nullptr,0);std::wstring s(static_cast<size_t>(len)+1,L'\0');DragQueryFileW(drop,i,s.data(),len+1);s.resize(len);files.emplace_back(s);}DragFinish(drop);p->addFiles(files);return 0;}
 case WM_TIMER:p->tick();return 0;
 case WM_CLOSE:KillTimer(h,1);p->managed.reset();p->neural.reset();p->treatment.reset();p->engine.reset();DestroyWindow(h);return 0;
 case WM_DESTROY:PostQuitMessage(p->smokeExit);return 0;
 }}catch(const std::exception& e){p->failure(e);if(p->smoke){p->smokeExit=1;PostMessageW(h,WM_CLOSE,0,0);}return 0;}
 return DefWindowProcW(h,m,w,l);
}
}
using namespace shiny::ui;using namespace shiny::player;
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);int result=1;bool automated=false;for(int i=1;i<argc;++i)if(std::wstring(argv[i])==L"--self-test"||std::wstring(argv[i])==L"--managed-test"||std::wstring(argv[i])==L"--ui-smoke")automated=true;
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Gdiplus::GdiplusStartupInput gdi;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&gdi,nullptr);
 try{
  if(argc==5&&std::wstring(argv[1])==L"--managed-test")result=managedPlaybackTest(argv[2],argv[3],argv[4]);
  else if(argc==5&&std::wstring(argv[1])==L"--self-test")result=playbackTest(argv[2],argv[3],argv[4]);
  else{
   INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);backgroundBrush=CreateSolidBrush(bg);panelBrush=CreateSolidBrush(panel);
   WNDCLASSW wc{};wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=backgroundBrush;wc.lpfnWndProc=windowProc;wc.lpszClassName=L"ShinyVlcPlayer";RegisterClassW(&wc);wc.lpfnWndProc=inputProc;wc.lpszClassName=L"ShinyPlayerInput";wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
   Window window;std::vector<std::filesystem::path> files;
   for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--vlc-dir"&&i+1<argc)window.runtimeFolder=argv[++i];else if(arg==L"--ui-smoke"&&i+2<argc){window.smoke=true;files.emplace_back(argv[++i]);window.smokeImage=argv[++i];}else if(arg.starts_with(L"--"))throw std::runtime_error("Unknown player option.");else files.emplace_back(arg);}
   HWND h=CreateWindowExW(0,L"ShinyVlcPlayer",L"Shiny Player",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,nullptr,nullptr,instance,&window);if(!h)throw std::runtime_error("Cannot create player window.");ShowWindow(h,show);UpdateWindow(h);if(!files.empty())window.addFiles(files);
   MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){try{if(window.key(msg))continue;}catch(const std::exception&e){window.failure(e);}if(!IsDialogMessageW(GetAncestor(msg.hwnd,GA_ROOT),&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}result=static_cast<int>(msg.wParam);
  }
 }catch(const std::exception& e){if(!automated)MessageBoxW(nullptr,wide(e.what()).c_str(),L"Shiny Player",MB_OK|MB_ICONERROR);}
 if(backgroundBrush)DeleteObject(backgroundBrush);if(panelBrush)DeleteObject(panelBrush);Gdiplus::GdiplusShutdown(token);CoUninitialize();LocalFree(argv);return result;
}
