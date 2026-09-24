// SPDX-License-Identifier: MIT
#include "client.hpp"
#include "builtins.hpp"
#include <algorithm>
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <future>
#include <optional>
#include <map>
#include "../ui/theme.hpp"
namespace {
using namespace shiny::packages;
std::wstring wide(std::string_view s){if(s.empty())return {};
int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
if(!n)return L"Invalid text";
std::wstring w(static_cast<std::size_t>(n),L'\0');
MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n);
return w;
}
std::optional<fs::path> pick(HWND owner,bool folder,bool save){
 IFileDialog* dialog=nullptr;
HRESULT made=save?CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)):CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog));
 if(FAILED(made))throw std::runtime_error("file-picker-unavailable");
 struct Release{IFileDialog* p;
~Release(){p->Release();
}} guard{dialog};
DWORD flags=0;
dialog->GetOptions(&flags);
dialog->SetOptions(flags|FOS_FORCEFILESYSTEM|(folder?FOS_PICKFOLDERS:0));
 if(save){dialog->SetDefaultExtension(L"json");
dialog->SetFileName(L"shiny-library-report.json");
}
 auto result=dialog->Show(owner);
if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};
if(FAILED(result))throw std::runtime_error("file-picker-failed");
 IShellItem* item=nullptr;
if(FAILED(dialog->GetResult(&item)))throw std::runtime_error("selection-unavailable");
PWSTR value=nullptr;
auto hr=item->GetDisplayName(SIGDN_FILESYSPATH,&value);
item->Release();
 if(FAILED(hr))throw std::runtime_error("selection-is-not-local");
fs::path path(value);
CoTaskMemFree(value);
return path;
}
struct Result {std::string report,message;
};
struct App {
 enum {List=100,Details=101,Status=102,Bundled=201,Import=202,Quarantine=203,Verify=204,Stage=205,Activate=206,Rollback=207,Original=208,Remove=209,Export=210,Refresh=211,Cancel=212};
 HWND hwnd=nullptr;
HFONT font=nullptr;
shiny::design::Theme theme;
UINT dpi=96;
bool closing=false,busy=false;
 std::future<Result> task;
std::stop_source cancellation;
std::string report,chosen;
fs::path root;
 struct Row{std::string id,version,state,digest,blocked;
bool compatible=false;
};
std::vector<Row> rows;
 ~App(){cancellation.request_stop();
if(task.valid())task.wait();

}
 int px(int x)const{return MulDiv(x,static_cast<int>(dpi),96);
}
 HWND control(const wchar_t* cls,const wchar_t* text,int id,DWORD style=0){auto h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,10,10,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
if(std::wstring_view(cls)==L"BUTTON")SetWindowSubclass(h,shiny::design::hoverProc,1,0);
if(std::wstring_view(cls)==L"STATIC")SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)&~WS_TABSTOP);
return h;
}
 void setFont(UINT value){theme.set(hwnd,value);dpi=theme.dpi;font=theme.body;
  if(auto list=GetDlgItem(hwnd,List))SendMessageW(list,LB_SETITEMHEIGHT,0,px(64));
  if(auto title=GetDlgItem(hwnd,1))SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(theme.heading),TRUE);
 }
 void create(){
  setFont(GetDpiForWindow(hwnd));
control(L"STATIC",L"Enhancement libraries",1);
  control(L"STATIC",L"Manage locally. Compare confidently. Original VLC and your media remain untouched.",2);
  control(L"LISTBOX",L"",List,LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS);
  control(L"EDIT",L"",Details,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL);
  const std::pair<int,const wchar_t*> buttons[]={{Bundled,L"Import bundled"},{Import,L"Import folder..."},{Quarantine,L"Quarantine DLL..."},{Verify,L"Verify"},{Stage,L"Stage"},{Activate,L"Select next preview"},{Rollback,L"Roll back"},{Original,L"Use original"},{Remove,L"Remove"},{Export,L"Export report..."},{Refresh,L"Refresh"},{Cancel,L"Cancel operation"}};
  for(auto [id,label]:buttons)control(L"BUTTON",label,id,BS_OWNERDRAW);
  control(L"STATIC",L"Opening local catalog...",Status);
control(L"STATIC",L"YOUR PACKAGES",3);control(L"STATIC",L"PACKAGE DETAILS & TRUST",4);
setFont(dpi);
SetTimer(hwnd,1,50,nullptr);
start(Refresh,{});
 }
 void layout(){RECT r{};GetClientRect(hwnd,&r);
  int w=MulDiv(r.right,96,static_cast<int>(dpi)),h=MulDiv(r.bottom,96,static_cast<int>(dpi));
  auto pos=[&](int id,int x,int y,int width,int height){MoveWindow(GetDlgItem(hwnd,id),px(x),px(y),px(std::max(1,width)),px(std::max(1,height)),TRUE);};
  pos(1,24,22,w-48,34);pos(2,25,66,w-50,32);
  int left=(w-64)*2/5,content=std::max(145,h-330);
  pos(3,24,115,left,22);pos(4,40+left,115,w-left-64,22);
  pos(List,24,145,left,content);pos(Details,40+left,145,w-left-64,content);
  int buttonWidth=(w-78)/4;
  for(int i=0;i<12;++i)pos(Bundled+i,24+(i%4)*(buttonWidth+10),h-165+(i/4)*39,buttonWidth,32);
  pos(Status,24,h-42,w-48,38);InvalidateRect(hwnd,nullptr,FALSE);
 }
 void drawRow(const DRAWITEMSTRUCT& d){
  FillRect(d.hDC,&d.rcItem,theme.card);if(d.itemID>=rows.size())return;
  const auto& row=rows[d.itemID];RECT box=d.rcItem;InflateRect(&box,-px(4),-px(4));
  bool selected=(d.itemState&ODS_SELECTED)!=0;
  theme.rounded(d.hDC,box,theme.surface,selected?theme.accent:theme.line,12);
  RECT title{box.left+px(12),box.top+px(7),box.right-px(12),box.top+px(28)};
  auto name=row.id=="shiny-spatial"?(row.version=="1.2.0"?"Adaptive detail":"Spatial reference"):row.id;
  theme.text(d.hDC,wide(name+"  "+row.version),title,font,theme.ink);
  RECT meta{title.left,title.bottom+px(2),title.right,box.bottom-px(4)};
  theme.text(d.hDC,wide(row.state+"  ·  "+(row.compatible?"Build catalog match":"Quarantined")),meta,theme.captionFont,selected?theme.accent:theme.muted);
  if(d.itemState&ODS_FOCUS){RECT focus=box;InflateRect(&focus,-px(2),-px(2));DrawFocusRect(d.hDC,&focus);}
 }
 std::string selectedId(){auto n=SendMessageW(GetDlgItem(hwnd,List),LB_GETCURSEL,0,0);
return n>=0&&static_cast<std::size_t>(n)<rows.size()?rows[static_cast<std::size_t>(n)].digest:std::string{};
}
 void enabled(){auto id=selectedId();
const Row* row=nullptr;
for(auto& r:rows)if(r.digest==id)row=&r;
  for(int c=Bundled;c<=Refresh;++c)EnableWindow(GetDlgItem(hwnd,c),!busy);
  EnableWindow(GetDlgItem(hwnd,List),!busy);
EnableWindow(GetDlgItem(hwnd,Cancel),busy);
  for(int c:{Verify,Stage,Activate,Remove})EnableWindow(GetDlgItem(hwnd,c),!busy&&row);
  EnableWindow(GetDlgItem(hwnd,Stage),!busy&&row&&row->compatible);
  EnableWindow(GetDlgItem(hwnd,Activate),!busy&&row&&row->compatible&&(row->state=="staged"||row->state=="selected"));
 }
 void showRows(){
  chosen=selectedId();
rows.clear();
Database db;
Statement q(db.get(),"SELECT json_extract(value,'$.id'),json_extract(value,'$.version'),json_extract(value,'$.state'),json_extract(value,'$.digest'),json_extract(value,'$.blockReason'),json_extract(value,'$.backendCompatible') FROM json_each(?,'$.packages')");
q.text(1,report);
  SendMessageW(GetDlgItem(hwnd,List),LB_RESETCONTENT,0,0);
int index=0,selection=0;
  while(q.step()){
   Row row{q.text(0),q.text(1),q.text(2),q.text(3),q.text(4),q.integer(5)!=0};
if(row.digest==chosen)selection=index;
   auto label=wide(row.id+" "+row.version+"  ["+row.state+"]  "+row.digest.substr(0,10));
SendMessageW(GetDlgItem(hwnd,List),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
rows.push_back(std::move(row));
++index;
  }
  if(!rows.empty())SendMessageW(GetDlgItem(hwnd,List),LB_SETCURSEL,selection,0);
details();
InvalidateRect(GetDlgItem(hwnd,List),nullptr,FALSE);
enabled();
 }
 void details(){
  auto id=selectedId();
std::string text="Selection applies only when a NEW managed preview starts.\r\nExisting previews keep their leased version until closed.\r\n\r\n";
  for(auto& row:rows)if(row.digest==id){text+="Package: "+row.id+"\r\nVersion: "+row.version+"\r\nState: "+row.state+"\r\nDigest: "+row.digest+"\r\n\r\n"+(row.compatible?"Manifest matches this build catalog. Verify bytes before staging. Not a neural or DLSS backend.":"Blocked: "+row.blocked+". Import never grants execution permission.")+"\r\n\r\n";
}
  text+="WORKFLOW\r\nImport → Verify → Stage → Select next preview\r\n\r\nThe adaptive detail package (1.2.0) limits halos and noise amplification. All three are CPU spatial filters, not DLSS.\r\n\r\nTRUST BOUNDARY\r\nThis unsigned build permits only its compiled first-party hashes. NVIDIA / Streamline candidates remain quarantined.\r\n\r\nExport report saves the complete machine-readable catalog. No full local paths or media names are included.";
  SetWindowTextW(GetDlgItem(hwnd,Details),wide(text).c_str());
enabled();
 }
 void start(int action,std::optional<fs::path> path){
  if(busy)return;
auto id=selectedId();
cancellation=std::stop_source{};
auto token=cancellation.get_token();
busy=true;
enabled();
SetWindowTextW(GetDlgItem(hwnd,Status),L"Working locally; original playback is unaffected...");
  task=std::async(std::launch::async,[action,path,id,token,root=root]()->Result{try{
   Store s(root.empty()?defaultStore():root,productionPolicy());std::string message="Catalog refreshed. No playback process was changed.";
   if(action==Bundled){for(auto version:{"1.0.0","1.1.0","1.2.0"})s.importFolder(executableDirectory()/"library-bundles"/version,token);message="Bundled packages imported into quarantine. Verify and stage before selection.";}
   else if(action==Import){s.importFolder(*path,token);message="Package imported into quarantine.";}
   else if(action==Quarantine){s.quarantineFile(*path,token);message="DLL copied into quarantine. It was not executed or approved.";}
   else if(action==Verify){auto reason=s.verify(id,token);message=reason.empty()?"Integrity and build policy verified.":"Integrity verified; activation blocked: "+reason;}
   else if(action==Stage){s.stage(id,token);message="Staged. Nothing is processing until you select and open a managed preview.";}
   else if(action==Activate){s.activate(id,probePackage,token);message="Known-frame probe passed. Selected for the NEXT managed preview.";}
   else if(action==Rollback){s.rollback(probePackage,token);message="Rollback completed; reopen a managed preview to apply.";}
   else if(action==Original){s.original();message="Original selected. Close any existing independent preview to stop its worker.";}
   else if(action==Remove){s.remove(id);message="Unselected, unleased package removed.";}
   return {s.list(),message};
  }catch(const fs::filesystem_error&){return {{},"Filesystem operation failed; source playback is unchanged."};}
   catch(const std::exception& e){return {{},std::string("Operation stopped: ")+e.what()};}
   catch(...){return {{},"Operation failed; source playback is unchanged."};}});
 }
 void tick(){if(!busy||!task.valid()||task.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
  auto result=task.get();
busy=false;
if(!result.report.empty()){report=std::move(result.report);
showRows();
}enabled();
SetWindowTextW(GetDlgItem(hwnd,Status),wide(result.message).c_str());
if(closing)DestroyWindow(hwnd);
 }
 void action(int id){
  if(id==Cancel){cancellation.request_stop();
return;
}if(busy)return;
  if(id==Import||id==Quarantine){auto p=pick(hwnd,id==Import,false);
if(p)start(id,p);
return;
}
  if(id==Export){auto p=pick(hwnd,false,true);
if(p){writeNew(*p,report+"\n");
SetWindowTextW(GetDlgItem(hwnd,Status),L"Report saved without absolute paths. Existing files are never overwritten.");
}return;
}
  if(id==Remove&&MessageBoxW(hwnd,L"Remove this unselected package from the managed store? Source files are not deleted.",L"Remove package",MB_OKCANCEL|MB_ICONWARNING)!=IDOK)return;
  if(id>=Bundled&&id<=Refresh)start(id,{});
 }
 static LRESULT CALLBACK proc(HWND h,UINT message,WPARAM w,LPARAM l){
  auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
app->hwnd=h;
SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
}
  if(!app)return DefWindowProcW(h,message,w,l);
  try{switch(message){case WM_CREATE:app->create();
return 0;
case WM_SIZE:app->layout();
return 0;
   case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={app->px(800),app->px(640)};
return 0;
   case WM_DPICHANGED:{app->setFont(HIWORD(w));
auto* r=reinterpret_cast<RECT*>(l);
SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);
return 0;
}
   case WM_SETTINGCHANGE:app->setFont(app->dpi);app->layout();return 0;
   case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,app->theme.background);return 1;}
   case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{bool card=reinterpret_cast<HWND>(l)==GetDlgItem(h,Details)||message==WM_CTLCOLORLISTBOX;return reinterpret_cast<LRESULT>(app->theme.control(reinterpret_cast<HDC>(w),card));}
   case WM_MEASUREITEM:reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=static_cast<UINT>(app->px(64));return TRUE;
   case WM_DRAWITEM:{auto& d=*reinterpret_cast<DRAWITEMSTRUCT*>(l);if(d.CtlID==List)app->drawRow(d);else app->theme.button(d,d.CtlID==Bundled||d.CtlID==Activate);return TRUE;}
   case WM_COMMAND:if(LOWORD(w)==List){if(HIWORD(w)==LBN_SELCHANGE)app->details();
}else app->action(LOWORD(w));
return 0;
   case WM_TIMER:app->tick();
return 0;
   case WM_CLOSE:if(app->busy){app->closing=true;
app->cancellation.request_stop();
SetWindowTextW(GetDlgItem(h,Status),L"Cancelling the operation before closing...");
}else DestroyWindow(h);
return 0;
   case WM_DESTROY:KillTimer(h,1);
PostQuitMessage(0);
return 0;
  }}catch(const std::exception& e){SetWindowTextW(GetDlgItem(h,Status),wide(e.what()).c_str());
}return DefWindowProcW(h,message,w,l);
 }
};
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_STANDARD_CLASSES};InitCommonControlsEx(&cc);
auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
if(FAILED(hr))return 2;
 int result=0;
try{App app;
 int argc=0;auto args=CommandLineToArgvW(GetCommandLineW(),&argc);
 if(!args)return 2;
 if(argc==3&&std::wstring_view(args[1])==L"--store-dir")app.root=args[2];
 else if(argc!=1){LocalFree(args);CoUninitialize();return 2;}
 LocalFree(args);
WNDCLASSW wc{};
wc.hInstance=instance;
wc.lpfnWndProc=App::proc;
wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
wc.hbrBackground=nullptr;
wc.lpszClassName=L"ShinyLibraryManager";
RegisterClassW(&wc);
  auto hwnd=CreateWindowExW(0,wc.lpszClassName,L"Shiny Player · Enhancement Libraries",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1120,740,nullptr,nullptr,instance,&app);
if(!hwnd)throw std::runtime_error("manager-window-failed");
ShowWindow(hwnd,show);
  MSG msg{};
while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);
DispatchMessageW(&msg);
}}
 }catch(...){result=2;
}CoUninitialize();
return result;
}
