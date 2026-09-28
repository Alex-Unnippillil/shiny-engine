// SPDX-License-Identifier: MIT
#include "bridge.hpp"
#include "service.hpp"
#include "../ui/theme.hpp"
#include <commdlg.h>
#include <shellapi.h>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
namespace shiny::swapper {
namespace {
constexpr wchar_t ClassName[]=L"ShinyOptionalSwapper";
constexpr wchar_t OwnerProperty[]=L"Shiny.OptionalSwapperWindow";
enum Id { Details=301, Warning=302, Consent=303, Browse=304, Forget=305, Launch=306, Status=307, Project5=308, ProjectClassic=309 };
struct Result { std::optional<Snapshot> snapshot; unsigned long pid=0; std::wstring error; };
std::wstring widen(std::string_view text){return std::wstring(text.begin(),text.end());}
struct State {
    HWND hwnd=nullptr,owner=nullptr;
    design::Theme theme;
    std::optional<Snapshot> selected;
    std::future<Result> pending;
    std::stop_source cancellation;
    bool busy=false,closing=false,forgetRequested=false,selfOwned=false;
    ~State(){cancellation.request_stop();if(pending.valid())pending.wait();}
    int px(int v)const{return theme.px(v);}
    HWND item(int id)const{return GetDlgItem(hwnd,id);}
    HWND control(const wchar_t* cls,const wchar_t* text,int id,DWORD style=0){
        auto h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,10,10,hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(theme.body),TRUE);
        if(std::wstring_view(cls)==L"BUTTON" && (style&BS_TYPEMASK)==BS_OWNERDRAW)SetWindowSubclass(h,design::hoverProc,1,0);
        return h;
    }
    void enable(){
        EnableWindow(item(Browse),!busy);EnableWindow(item(Consent),!busy&&selected.has_value());
        EnableWindow(item(Launch),!busy&&selected&&SendMessageW(item(Consent),BM_GETCHECK,0,0)==BST_CHECKED);
        SetWindowTextW(item(Forget),busy?L"Cancel check":L"Forget selection");
    }
    void reset(){selected.reset();SendMessageW(item(Consent),BM_SETCHECK,BST_UNCHECKED,0);
        SetWindowTextW(item(Details),L"No executable selected. Nothing is enabled or launched automatically.");enable();}
    void create(){
        theme.set(hwnd);control(L"STATIC",L"Optional external swapper",300);
        SendMessageW(item(300),WM_SETFONT,reinterpret_cast<WPARAM>(theme.heading),TRUE);
        control(L"STATIC",L"Windows tools, launched separately at your request.\r\nThis does not enable DLSS 5 in Shiny, VLC or the Qt video pipeline.",310,SS_NOPREFIX);
        control(L"EDIT",L"",Details,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP);
        control(L"STATIC",L"THIRD-PARTY CODE — NOT SANDBOXED\r\nOnly the selected EXE is fingerprinted; companion files are not verified. A matching name or hash is not publisher approval. The tool may access files and network services with your account permissions.\r\n\r\nNo media path or install target is passed. Do not target Shiny/VLC installations. Review the tool's own compatibility, backup/restore and anti-cheat warnings. Closing Shiny does not undo external changes.",Warning,SS_NOPREFIX);
        control(L"BUTTON",L"I trust this selected application and understand that it runs independently.\r\nLaunch permission applies once, to the displayed executable fingerprint.",Consent,BS_AUTOCHECKBOX|BS_MULTILINE|WS_TABSTOP);
        control(L"BUTTON",L"Choose executable…",Browse,BS_OWNERDRAW|WS_TABSTOP);
        control(L"BUTTON",L"Forget selection",Forget,BS_OWNERDRAW|WS_TABSTOP);
        control(L"BUTTON",L"Launch external tool",Launch,BS_OWNERDRAW|WS_TABSTOP);
        control(L"BUTTON",L"DLSS 5 project",Project5,BS_OWNERDRAW|WS_TABSTOP);
        control(L"BUTTON",L"Classic project",ProjectClassic,BS_OWNERDRAW|WS_TABSTOP);
        control(L"BUTTON",L"Close",IDCANCEL,BS_OWNERDRAW|WS_TABSTOP);
        control(L"STATIC",L"Choose an installed or portable application you lawfully obtained. Setup executables and scripts are not accepted.",Status,SS_NOPREFIX);
        reset();SetTimer(hwnd,1,50,nullptr);
    }
    void layout(){
        RECT r{};GetClientRect(hwnd,&r);const int w=MulDiv(r.right,96,theme.dpi);
        auto move=[&](int id,int x,int y,int width,int height){MoveWindow(item(id),px(x),px(y),px(width),px(height),TRUE);};
        move(300,24,18,w-48,36);move(310,24,64,w-48,50);
        move(Details,24,126,w-48,126);move(Warning,24,264,w-48,126);
        move(Consent,24,404,w-48,46);
        move(Browse,24,464,190,36);move(Forget,226,464,170,36);move(Launch,w-224,464,200,36);
        move(Project5,24,512,160,34);move(ProjectClassic,194,512,150,34);move(IDCANCEL,w-124,512,100,34);
        move(Status,24,560,w-48,54);
    }
    void choose(){
        if(busy)return;
        wchar_t path[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=hwnd;
        ofn.lpstrFilter=L"Installed or portable swapper (*.exe)\0*.exe\0\0";ofn.lpstrFile=path;ofn.nMaxFile=32768;
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_DONTADDTORECENT;
        ofn.lpstrTitle=L"Select DLSS 5 Swapper or classic DLSS Swapper (not setup)";
        // A new picker always revokes any prior selection, even if cancelled.
        reset();
        if(!GetOpenFileNameW(&ofn))return;
        work(std::filesystem::path(path),false);
    }
    void work(const std::filesystem::path& path,bool execute){
        if(busy || (execute && (!selected || SendMessageW(item(Consent),BM_GETCHECK,0,0)!=BST_CHECKED)))return;
        auto consentSnapshot=selected;
        SendMessageW(item(Consent),BM_SETCHECK,BST_UNCHECKED,0);
        busy=true;forgetRequested=false;cancellation=std::stop_source{};const auto token=cancellation.get_token();enable();
        SetWindowTextW(item(Status),execute?L"Rechecking the selected bytes before starting. Cancel stops pending work.":L"Inspecting without execution. Playback stays available; Cancel stops the check.");
        try { pending=std::async(std::launch::async,[path,execute,consentSnapshot,token](){
            Result r;
            try{if(execute)r.pid=launch(*consentSnapshot,true,token);else r.snapshot=inspect(path,token);}
            catch(const std::exception& e){r.error=widen(e.what());}catch(...){r.error=L"external-tool-operation-failed";}
            return r;
        }); } catch(...) { busy=false;reset();throw; }
    }
    void tick(){
        if(!busy || !pending.valid() || pending.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
        auto result=pending.get();busy=false;
        if(closing){DestroyWindow(hwnd);return;}
        if(forgetRequested){reset();SetWindowTextW(item(Status),L"Selection and launch consent cleared. Any already-started external process remains independent.");return;}
        if(result.snapshot){
            selected=std::move(result.snapshot);
            const auto text=std::wstring(title(selected->tool))+L"\r\n"+selected->path.wstring()+L"\r\nSHA-256: "+widen(selected->sha256)+L"\r\n"+std::to_wstring(selected->bytes)+L" bytes · Windows "+widen(selected->architecture)+L" EXE · Publisher/companion files NOT verified";
            SetWindowTextW(item(Details),text.c_str());
            SetWindowTextW(item(Status),L"Inspection complete. Review the exact file and warnings; consent is unchecked. No code has been executed.");
        }else{
            reset();
            const auto message=result.pid?L"External process started (PID "+std::to_wstring(result.pid)+L"). This is NOT a DLSS-active or compatibility status. Selection cleared.":L"Not launched: "+result.error;
            SetWindowTextW(item(Status),message.c_str());
        }
        enable();
    }
};
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    auto* s=reinterpret_cast<State*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){s=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);s->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}
    if(!s)return DefWindowProcW(h,m,w,l);
    try{switch(m){
        case WM_CREATE:s->create();s->layout();return 0;
        case WM_SIZE:s->layout();return 0;
        case WM_GETMINMAXINFO:{RECT r{0,0,s->px(800),s->px(628)};AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_CONTROLPARENT,s->theme.dpi);reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={r.right-r.left,r.bottom-r.top};return 0;}
        case WM_DPICHANGED:{s->theme.set(h,HIWORD(w));auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);s->layout();return 0;}
        case WM_SETTINGCHANGE:s->theme.set(h);s->layout();return 0;
        case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,s->theme.background);return 1;}
        case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORBTN:return reinterpret_cast<LRESULT>(s->theme.control(reinterpret_cast<HDC>(w),reinterpret_cast<HWND>(l)==s->item(Details)));
        case WM_DRAWITEM:s->theme.button(*reinterpret_cast<DRAWITEMSTRUCT*>(l),w==Launch);return TRUE;
        case WM_TIMER:s->tick();return 0;
        case WM_COMMAND:switch(LOWORD(w)){
            case Browse:s->choose();return 0;
            case Consent:s->enable();return 0;
            case Forget:if(s->busy){s->forgetRequested=true;s->cancellation.request_stop();}else{s->reset();SetWindowTextW(s->item(Status),L"Selection cleared. Nothing is saved for the next session.");}return 0;
            case Launch:if(s->selected)s->work(s->selected->path,true);return 0;
            case Project5:case ProjectClassic:{
                const wchar_t* url=LOWORD(w)==Project5?L"https://github.com/rakanki911/DLSS5-Swapper":L"https://github.com/beeradmoore/dlss-swapper";
                if(reinterpret_cast<INT_PTR>(ShellExecuteW(h,L"open",url,nullptr,nullptr,SW_SHOWNORMAL))<=32)SetWindowTextW(s->item(Status),L"Could not open the project in the default browser.");
                return 0;
            }
            case IDCANCEL:SendMessageW(h,WM_CLOSE,0,0);return 0;
        }break;
        case WM_CLOSE:if(s->busy){s->closing=true;s->cancellation.request_stop();ShowWindow(h,SW_HIDE);}else DestroyWindow(h);return 0;
        case WM_NCDESTROY:
            KillTimer(h,1);if(IsWindow(s->owner)&&GetPropW(s->owner,OwnerProperty)==h)RemovePropW(s->owner,OwnerProperty);
            SetWindowLongPtrW(h,GWLP_USERDATA,0);if(s->selfOwned)delete s;return DefWindowProcW(h,m,w,l);
    }}catch(const std::exception& e){SetWindowTextW(s->item(Status),widen(e.what()).c_str());}
    return DefWindowProcW(h,m,w,l);
}
}
void open(void* nativeOwner){
    auto owner=static_cast<HWND>(nativeOwner);
    if(auto old=static_cast<HWND>(GetPropW(owner,OwnerProperty));old&&IsWindow(old)){ShowWindow(old,SW_SHOWNORMAL);SetForegroundWindow(old);return;}
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=ClassName;
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("swapper-window-unavailable");
    auto state=std::make_unique<State>();state->owner=owner;
    HWND h=CreateWindowExW(WS_EX_CONTROLPARENT,ClassName,L"Optional DLSS 5 Swapper — external tool",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,870,700,owner,nullptr,wc.hInstance,state.get());
    if(!h)throw std::runtime_error("swapper-window-unavailable");
    state->selfOwned=true;state.release();
    if(owner)SetPropW(owner,OwnerProperty,h);
    ShowWindow(h,SW_SHOWNORMAL);SetFocus(GetDlgItem(h,Browse));
}
bool translate(void* nativeMessage){
    auto* msg=static_cast<MSG*>(nativeMessage);
    if(msg->message<WM_KEYFIRST || msg->message>WM_KEYLAST)return false;
    auto root=GetAncestor(msg->hwnd,GA_ROOT);wchar_t name[64]{};GetClassNameW(root,name,64);
    if(std::wstring_view(name)!=ClassName)return false;
    if(msg->message==WM_KEYDOWN&&msg->wParam==VK_ESCAPE){SendMessageW(root,WM_CLOSE,0,0);return true;}
    return IsDialogMessageW(root,msg)!=FALSE;
}
}
