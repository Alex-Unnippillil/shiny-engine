// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <algorithm>
#include <string>
#include <string_view>

namespace shiny::design {
// Native buttons retain keyboard, accessibility and focus behavior; only paint changes.
inline LRESULT CALLBACK hoverProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_MOUSEMOVE && !GetPropW(h, L"Shiny.Hover")) {
        SetPropW(h, L"Shiny.Hover", reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0}; TrackMouseEvent(&t);
        InvalidateRect(h, nullptr, FALSE);
    } else if (m == WM_MOUSELEAVE || m == WM_KILLFOCUS || m == WM_ENABLE) {
        RemovePropW(h, L"Shiny.Hover"); InvalidateRect(h, nullptr, FALSE);
    } else if (m == WM_NCDESTROY) {
        RemovePropW(h, L"Shiny.Hover"); RemoveWindowSubclass(h, hoverProc, 1);
    }
    return DefSubclassProc(h, m, w, l);
}
struct Theme {
    UINT dpi=96;
    COLORREF bg{}, surface{}, ink{}, muted{}, accent{}, line{};
    HBRUSH background=nullptr, card=nullptr;
    HFONT body=nullptr, heading=nullptr, small=nullptr;
    bool contrast=false;
    ~Theme(){clear();}
    Theme()=default;
    Theme(const Theme&)=delete; Theme& operator=(const Theme&)=delete;
    int px(int x) const {return MulDiv(x, static_cast<int>(dpi), 96);}
    void clear(){for(auto f:{body,heading,small})if(f)DeleteObject(f); body=heading=small=nullptr;
        if(background)DeleteObject(background);if(card)DeleteObject(card);background=card=nullptr;}
    HFONT font(int size,int weight)const{return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");}
    void set(HWND window,UINT value=0){clear();dpi=value?value:GetDpiForWindow(window);if(!dpi)dpi=96;
        HIGHCONTRASTW hc{};hc.cbSize=sizeof(hc);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);
        contrast=(hc.dwFlags&HCF_HIGHCONTRASTON)!=0;
        bg=contrast?GetSysColor(COLOR_WINDOW):RGB(13,17,25);
        surface=contrast?GetSysColor(COLOR_WINDOW):RGB(23,30,42);
        ink=contrast?GetSysColor(COLOR_WINDOWTEXT):RGB(234,240,249);
        muted=contrast?ink:RGB(167,181,201);
        accent=contrast?GetSysColor(COLOR_HIGHLIGHT):RGB(110,221,198);
        line=contrast?ink:RGB(54,68,89);
        background=CreateSolidBrush(bg);card=CreateSolidBrush(surface);
        body=font(14,FW_NORMAL);heading=font(25,FW_SEMIBOLD);small=font(12,FW_NORMAL);
        EnumChildWindows(window,[](HWND c,LPARAM p)->BOOL{
            SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(reinterpret_cast<Theme*>(p)->body),TRUE);return TRUE;
        },reinterpret_cast<LPARAM>(this));
        BOOL dark=contrast?FALSE:TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
        DWORD corners=2;DwmSetWindowAttribute(window,33,&corners,sizeof(corners));
        InvalidateRect(window,nullptr,TRUE);
    }
    void text(HDC dc,std::wstring_view value,RECT r,HFONT f,COLORREF color,UINT flags=DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS)const{
        auto previous=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
        DrawTextW(dc,value.data(),static_cast<int>(value.size()),&r,flags);SelectObject(dc,previous);
    }
    void rounded(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius=10)const{
        auto brush=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,1,border);
        auto oldB=SelectObject(dc,brush),oldP=SelectObject(dc,pen);
        RoundRect(dc,r.left,r.top,r.right,r.bottom,px(radius),px(radius));
        SelectObject(dc,oldB);SelectObject(dc,oldP);DeleteObject(brush);DeleteObject(pen);
    }
    void button(const DRAWITEMSTRUCT& d,bool primary=false)const{
        if(d.CtlType!=ODT_BUTTON)return;
        bool disabled=(d.itemState&ODS_DISABLED)!=0,pressed=(d.itemState&ODS_SELECTED)!=0;
        bool hovered=GetPropW(d.hwndItem,L"Shiny.Hover")!=nullptr;
        COLORREF fill=primary&&!disabled?accent:surface;
        if(!contrast&&(pressed||hovered)&&!disabled)fill=primary?RGB(82,192,170):RGB(38,51,69);
        FillRect(d.hDC,&d.rcItem,background);RECT r=d.rcItem;InflateRect(&r,-1,-1);
        rounded(d.hDC,r,fill,disabled?line:primary?accent:line);
        wchar_t label[256]{};GetWindowTextW(d.hwndItem,label,256);
        COLORREF color=disabled?(contrast?GetSysColor(COLOR_GRAYTEXT):muted):primary?(contrast?GetSysColor(COLOR_HIGHLIGHTTEXT):bg):ink;
        text(d.hDC,label,r,body,color,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if((d.itemState&ODS_FOCUS)&&!(d.itemState&ODS_NOFOCUSRECT)){
            InflateRect(&r,-px(4),-px(4));SetTextColor(d.hDC,color);DrawFocusRect(d.hDC,&r);
        }
    }
    HBRUSH control(HDC dc,bool panel=false)const{
        SetTextColor(dc,ink);SetBkColor(dc,panel?surface:bg);return panel?card:background;
    }
    void header(HDC dc,int width,const wchar_t* title,const wchar_t* subtitle)const{
        text(dc,title,{px(24),px(20),width-px(24),px(53)},heading,ink);
        text(dc,subtitle,{px(25),px(62),width-px(24),px(84)},body,muted);
    }
};
}
