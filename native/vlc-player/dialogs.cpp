// SPDX-License-Identifier: MIT
#include "ui.hpp"
namespace shiny::ui {
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
std::optional<std::wstring> input(HWND parent,const wchar_t* title,const wchar_t* label,const wchar_t* initial){
 Input p;p.label=label;p.initial=initial;
 RECT r{};GetWindowRect(parent,&r);
 EnableWindow(parent,FALSE);
 HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,L"ShinyPlayerInput",title,WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,
  r.left+70,r.top+80,600,190,parent,nullptr,GetModuleHandleW(nullptr),&p);
 if(!h){EnableWindow(parent,TRUE);throw std::runtime_error("Could not open dialog.");}
 MSG msg{};while(!p.done){BOOL rc=GetMessageW(&msg,nullptr,0,0);if(rc<=0){if(rc==0)PostQuitMessage(static_cast<int>(msg.wParam));break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetForegroundWindow(parent);return p.result;
}
std::vector<std::filesystem::path> pick(HWND parent,bool multiple,bool folder,bool save,const wchar_t* ext){
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
}
