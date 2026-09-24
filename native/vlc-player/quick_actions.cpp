// SPDX-License-Identifier: MIT
#include "quick_actions.hpp"
#include "workspace_model.hpp"
#include "../ui/theme.hpp"
namespace shiny::ui {
namespace {
constexpr int Search = 10, Results = 11, Hint = 12;
struct Palette {
    HWND hwnd = nullptr;
    design::Theme theme;
    const std::vector<QuickAction>& actions;
    std::vector<std::size_t> visible;
    std::optional<int> result;
    explicit Palette(const std::vector<QuickAction>& source) : actions(source) {}
    void refresh() {
        wchar_t query[257]{}; GetWindowTextW(GetDlgItem(hwnd, Search), query, 257);
        visible.clear(); auto list = GetDlgItem(hwnd, Results);
        SendMessageW(list, WM_SETREDRAW, FALSE, 0); SendMessageW(list, LB_RESETCONTENT, 0, 0);
        for (std::size_t i = 0; i < actions.size(); ++i) {
            if (!player::matchesTitle(actions[i].title + L" " + actions[i].detail, query)) continue;
            visible.push_back(i);
            auto label = actions[i].title + L" — " + (actions[i].enabled ? actions[i].detail : L"Open suitable media first");
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        if (!visible.empty()) SendMessageW(list, LB_SETCURSEL, 0, 0);
        SendMessageW(list, WM_SETREDRAW, TRUE, 0); InvalidateRect(list, nullptr, TRUE); selection();
    }
    const QuickAction* selected() const {
        auto at = SendMessageW(GetDlgItem(hwnd, Results), LB_GETCURSEL, 0, 0);
        return at >= 0 && static_cast<std::size_t>(at) < visible.size() ? &actions[visible[static_cast<std::size_t>(at)]] : nullptr;
    }
    void selection() {
        auto item = selected();
        EnableWindow(GetDlgItem(hwnd, IDOK), item && item->enabled);
        SetWindowTextW(GetDlgItem(hwnd, Hint), !item ? L"No commands match. Try playback, queue, or video."
            : !item->enabled ? L"This command needs suitable media. Open a local video first."
            : L"↑ ↓ to choose   ·   Enter to run   ·   Esc to dismiss");
    }
    void run() { auto item = selected(); if (item && item->enabled) { result = item->id; DestroyWindow(hwnd); } }
    void layout() {
        RECT r{}; GetClientRect(hwnd, &r); const auto p = [&](int x) { return theme.px(x); };
        auto move = [&](int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(hwnd,id),p(x),p(y),p(w),p(h),TRUE); };
        int width = MulDiv(r.right,96,static_cast<int>(theme.dpi));
        int height = MulDiv(r.bottom,96,static_cast<int>(theme.dpi));
        move(Search,24,98,width-48,32); move(Results,24,146,width-48,std::max(30,height-264));
        move(Hint,24,height-104,width-48,32); move(IDCANCEL,width-242,height-58,102,36); move(IDOK,width-128,height-58,104,36);
    }
    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<Palette*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if (m == WM_NCCREATE) { self = static_cast<Palette*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); self->hwnd=h; SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
        if (!self) return DefWindowProcW(h,m,w,l);
        try {
            switch (m) {
            case WM_CREATE: {
                auto child = [&](const wchar_t* cls, const wchar_t* text, int id, DWORD style) {
                    return CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,h,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
                };
                auto edit=child(L"EDIT",L"",Search,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL);
                SendMessageW(edit,EM_SETLIMITTEXT,256,0);
                SendMessageW(edit,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Find a command…"));
                child(L"LISTBOX",L"Commands",Results,WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS);
                child(L"STATIC",L"",Hint,0);
                for (auto pair : {std::pair{IDCANCEL,L"Cancel"},std::pair{IDOK,L"Run command"}}) {
                    auto b=child(L"BUTTON",pair.second,pair.first,WS_TABSTOP|BS_OWNERDRAW); SetWindowSubclass(b,design::hoverProc,1,0);
                }
                self->theme.set(h); SendMessageW(GetDlgItem(h,Results),LB_SETITEMHEIGHT,0,self->theme.px(60));
                self->layout(); self->refresh(); return 0;
            }
            case WM_SIZE: self->layout(); return 0;
            case WM_SETTINGCHANGE: self->theme.set(h,self->theme.dpi); InvalidateRect(h,nullptr,TRUE); return 0;
            case WM_DPICHANGED: {
                self->theme.set(h,HIWORD(w)); auto* r=reinterpret_cast<RECT*>(l);
                SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);
                SendMessageW(GetDlgItem(h,Results),LB_SETITEMHEIGHT,0,self->theme.px(60)); self->layout(); return 0;
            }
            case WM_MEASUREITEM: reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=60; return TRUE;
            case WM_DRAWITEM: {
                const auto& d=*reinterpret_cast<DRAWITEMSTRUCT*>(l);
                if (d.CtlType==ODT_BUTTON) { self->theme.button(d,d.CtlID==IDOK); return TRUE; }
                FillRect(d.hDC,&d.rcItem,self->theme.card);
                if (d.itemID>=self->visible.size()) return TRUE;
                auto& item=self->actions[self->visible[d.itemID]]; auto& t=self->theme; RECT box=d.rcItem; InflateRect(&box,-t.px(4),-t.px(3));
                t.rounded(d.hDC,box,t.surface,(d.itemState&ODS_SELECTED)?t.accent:t.line);
                RECT title{box.left+t.px(12),box.top+t.px(6),box.right-t.px(85),box.top+t.px(28)};
                t.text(d.hDC,item.title,title,t.body,item.enabled?t.ink:t.muted);
                t.text(d.hDC,item.shortcut,{title.right, title.top,box.right-t.px(10),title.bottom},t.captionFont,t.muted,DT_RIGHT|DT_SINGLELINE);
                t.text(d.hDC,item.enabled?item.detail:L"Open suitable media first",{title.left,title.bottom,box.right-t.px(12),box.bottom},t.captionFont,t.muted);
                if (d.itemState&ODS_FOCUS) { InflateRect(&box,-t.px(2),-t.px(2)); DrawFocusRect(d.hDC,&box); } return TRUE;
            }
            case WM_ERASEBKGND: {RECT r{};GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,self->theme.background);return 1;}
            case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
                return reinterpret_cast<LRESULT>(self->theme.control(reinterpret_cast<HDC>(w),m!=WM_CTLCOLORSTATIC));
            case WM_PAINT: {PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);self->theme.header(dc,r.right,L"Quick actions",L"Search existing controls. Nothing runs until you choose a command.");EndPaint(h,&ps);return 0;}
            case WM_COMMAND:
                if (LOWORD(w)==Search && HIWORD(w)==EN_CHANGE) self->refresh();
                else if (LOWORD(w)==Results && HIWORD(w)==LBN_SELCHANGE) self->selection();
                else if (LOWORD(w)==IDOK || (LOWORD(w)==Results && HIWORD(w)==LBN_DBLCLK)) self->run();
                else if (LOWORD(w)==IDCANCEL) DestroyWindow(h);
                return 0;
            case WM_CLOSE: DestroyWindow(h); return 0;
            }
        } catch (...) { DestroyWindow(h); return 0; }
        return DefWindowProcW(h,m,w,l);
    }
};
}
std::optional<int> quickActions(HWND owner,const std::vector<QuickAction>& actions) {
    Palette palette(actions); WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=Palette::proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"ShinyQuickActions";
    RegisterClassW(&wc);
    const auto focus=GetFocus(); const bool enabled=IsWindowEnabled(owner)!=FALSE;
    RECT anchor{};GetWindowRect(owner,&anchor);UINT dpi=GetDpiForWindow(owner); if(!dpi)dpi=96;
    RECT size{0,0,MulDiv(660,static_cast<int>(dpi),96),MulDiv(532,static_cast<int>(dpi),96)};
    AdjustWindowRectExForDpi(&size,WS_CAPTION|WS_SYSMENU|WS_POPUP,FALSE,WS_EX_DLGMODALFRAME,dpi);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);
    int width=static_cast<int>(std::min(size.right-size.left,monitor.rcWork.right-monitor.rcWork.left));
    int height=static_cast<int>(std::min(size.bottom-size.top,monitor.rcWork.bottom-monitor.rcWork.top));
    int x=std::max(monitor.rcWork.left,std::min(anchor.left+(anchor.right-anchor.left-width)/2,monitor.rcWork.right-width));
    int y=std::max(monitor.rcWork.top,std::min(anchor.top+40,monitor.rcWork.bottom-height));
    EnableWindow(owner,FALSE);
    auto hwnd=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"Quick actions · Shiny Player",WS_CAPTION|WS_SYSMENU|WS_POPUP,x,y,width,height,owner,nullptr,wc.hInstance,&palette);
    if (hwnd) {
        ShowWindow(hwnd,SW_SHOW); SetFocus(GetDlgItem(hwnd,Search));
        MSG msg{};
        while (IsWindow(hwnd)) {
            const auto result=GetMessageW(&msg,nullptr,0,0);
            if(result<=0) {if(result==0)PostQuitMessage(static_cast<int>(msg.wParam));break;}
            if(GetAncestor(msg.hwnd,GA_ROOT)==hwnd && msg.message==WM_KEYDOWN) {
                if(msg.wParam==VK_ESCAPE){DestroyWindow(hwnd);continue;}
                if(msg.wParam==VK_RETURN){if(msg.hwnd==GetDlgItem(hwnd,IDCANCEL))DestroyWindow(hwnd);else palette.run();continue;}
                if(msg.hwnd==GetDlgItem(hwnd,Search) && (msg.wParam==VK_DOWN||msg.wParam==VK_UP)){
                    auto list=GetDlgItem(hwnd,Results);auto at=SendMessageW(list,LB_GETCURSEL,0,0);auto count=static_cast<LRESULT>(palette.visible.size());
                    if(count){at=std::clamp<LRESULT>(at+(msg.wParam==VK_DOWN?1:-1),0,count-1);SendMessageW(list,LB_SETCURSEL,at,0);palette.selection();}continue;
                }
            }
            if(!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        }
        if(IsWindow(hwnd))DestroyWindow(hwnd);
    }
    if(IsWindow(owner)){EnableWindow(owner,enabled);SetForegroundWindow(owner);if(IsWindow(focus)&&IsWindowVisible(focus)&&IsWindowEnabled(focus))SetFocus(focus);}
    if(!hwnd)throw std::runtime_error("Quick actions could not open.");
    return palette.result;
}
}
