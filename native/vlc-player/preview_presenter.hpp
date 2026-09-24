// SPDX-License-Identifier: MIT
#pragma once
#include "preview_geometry.hpp"
#include <windows.h>
#include <d2d1.h>
#include <gdiplus.h>
#include <wrl/client.h>
#include <span>
#include <string>

namespace shiny::preview {
// One matched pair, one pending upload; no frame-sized allocations during repaint.
// Processing remains CPU/SDR. Direct2D can choose hardware or software presentation.
class Presenter {
    using Bytes=std::vector<std::uint8_t>;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> beforeBitmap,afterBitmap;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Bytes before,after;
    unsigned width=0,height=0;
    bool dirty=true,compatibility=false,lastDirect=false;
    static Bytes premultiply(std::span<const std::uint8_t> rgba){
        Bytes result(rgba.size());
        for(std::size_t i=0;i<rgba.size();i+=4){unsigned a=rgba[i+3];
            result[i]=static_cast<std::uint8_t>((rgba[i+2]*a+127)/255);
            result[i+1]=static_cast<std::uint8_t>((rgba[i+1]*a+127)/255);
            result[i+2]=static_cast<std::uint8_t>((rgba[i]*a+127)/255);result[i+3]=static_cast<std::uint8_t>(a);
        }return result;
    }
    void release(){beforeBitmap.Reset();afterBitmap.Reset();brush.Reset();target.Reset();dirty=true;lastDirect=false;}
    static D2D1_RECT_F box(Rect r){return D2D1::RectF(r.x,r.y,r.x+r.w,r.y+r.h);}
    static Gdiplus::RectF gbox(Rect r){return {r.x,r.y,r.w,r.h};}
    std::array<Rect,2> areas()const{
        RECT r{};GetClientRect(hwnd,&r);float w=static_cast<float>(r.right),h=static_cast<float>(r.bottom);
        if(mode==0){float half=std::max(1.f,(w-12.f)*.5f);return {Rect{0,0,half,h},Rect{half+12,0,half,h}};}
        return {Rect{0,0,w,h},Rect{0,0,w,h}};
    }
    bool prepare(){
        if(compatibility)return false;
        if(!factory&&FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf())))return false;
        RECT r{};GetClientRect(hwnd,&r);if(r.right<=0||r.bottom<=0)return false;
        if(!target){
            auto properties=D2D1::RenderTargetProperties();properties.dpiX=properties.dpiY=96;
            if(FAILED(factory->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(hwnd,D2D1::SizeU(static_cast<UINT32>(r.right),static_cast<UINT32>(r.bottom))),target.GetAddressOf())))return false;
            if(FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.43f,.87f,.78f),brush.GetAddressOf()))){release();return false;}
        }
        if(!width||before.empty())return true;
        if(dirty){
            auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
            auto update=[&](Microsoft::WRL::ComPtr<ID2D1Bitmap>& bitmap,const Bytes& data){
                if(bitmap){auto size=bitmap->GetPixelSize();if(size.width!=width||size.height!=height)bitmap.Reset();}
                return bitmap?bitmap->CopyFromMemory(nullptr,data.data(),width*4):target->CreateBitmap(D2D1::SizeU(width,height),data.data(),width*4,properties,bitmap.GetAddressOf());
            };
            if(FAILED(update(beforeBitmap,before))||FAILED(update(afterBitmap,after))){release();return false;}
            dirty=false;
        }return true;
    }
    bool direct(){
        if(!prepare())return false;
        target->BeginDraw();target->Clear(D2D1::ColorF(.035f,.047f,.07f));
        if(beforeBitmap&&afterBitmap){auto a=areas();
            auto draw=[&](ID2D1Bitmap* image,Rect area,Rect clip){target->PushAxisAlignedClip(box(clip),D2D1_ANTIALIAS_MODE_ALIASED);
                target->DrawBitmap(image,box(fit(area,width,height,actual)),1.f,actual?D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR:D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                target->PopAxisAlignedClip();};
            if(mode==0){draw(beforeBitmap.Get(),a[0],a[0]);draw(afterBitmap.Get(),a[1],a[1]);}
            else if(mode==1){draw(beforeBitmap.Get(),a[0],a[0]);auto clip=a[0];clip.x+=clip.w*split;clip.w*=1.f-split;draw(afterBitmap.Get(),a[0],clip);
                float x=a[0].w*split;target->DrawLine(D2D1::Point2F(x,0),D2D1::Point2F(x,a[0].h),brush.Get(),2.f);}
            else draw(mode==2?afterBitmap.Get():beforeBitmap.Get(),a[0],a[0]);
        }
        if(GetFocus()==hwnd){RECT r{};GetClientRect(hwnd,&r);
            target->DrawRectangle(D2D1::RectF(1,1,static_cast<float>(r.right-1),static_cast<float>(r.bottom-1)),brush.Get(),2.f);}
        auto hr=target->EndDraw();if(FAILED(hr)){release();return false;}lastDirect=beforeBitmap&&afterBitmap;return true;
    }
    void fallback(HDC dc){
        Gdiplus::Graphics g(dc);g.Clear(Gdiplus::Color(255,9,12,18));
        if(before.empty())return;
        auto a=areas();g.SetInterpolationMode(actual?Gdiplus::InterpolationModeNearestNeighbor:Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::ImageAttributes attrs;attrs.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        auto draw=[&](Bytes& data,Rect area,Rect clip){auto state=g.Save();g.SetClip(gbox(clip));
            Gdiplus::Bitmap image(static_cast<INT>(width),static_cast<INT>(height),static_cast<INT>(width*4),PixelFormat32bppPARGB,data.data());
            g.DrawImage(&image,gbox(fit(area,width,height,actual)),0,0,static_cast<float>(width),static_cast<float>(height),Gdiplus::UnitPixel,&attrs);g.Restore(state);};
        if(mode==0){draw(before,a[0],a[0]);draw(after,a[1],a[1]);}
        else if(mode==1){draw(before,a[0],a[0]);auto clip=a[0];clip.x+=clip.w*split;clip.w*=1.f-split;draw(after,a[0],clip);
            Gdiplus::Pen pen(Gdiplus::Color(255,110,221,198),2);g.DrawLine(&pen,a[0].w*split,0.f,a[0].w*split,a[0].h);}
        else draw(mode==2?after:before,a[0],a[0]);
        if(GetFocus()==hwnd){RECT r{};GetClientRect(hwnd,&r);Gdiplus::Pen pen(Gdiplus::Color(255,110,221,198),2);
            g.DrawRectangle(&pen,1,1,static_cast<INT>(r.right-2),static_cast<INT>(r.bottom-2));}
    }
    void announce(){
        auto text=mode==1?L"Wipe comparison: "+std::to_wstring(static_cast<int>(std::lround(split*100)))+L"% source; Left/Right adjust, Home/End endpoints":
            mode==2?std::wstring(L"Processed image, local SDR filter"):mode==3?std::wstring(L"Original decoded source image"):std::wstring(L"Matched source on left and processed image on right");
        SetWindowTextW(hwnd,text.c_str());
    }
    void pointer(LPARAM l){RECT r{};GetClientRect(hwnd,&r);split=wipe(static_cast<float>(static_cast<short>(LOWORD(l)))/static_cast<float>(std::max(1L,r.right)));announce();InvalidateRect(hwnd,nullptr,FALSE);}
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){auto* self=reinterpret_cast<Presenter*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Presenter*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->hwnd=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(h,m,w,l);
        switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(h,nullptr,FALSE);return 0;
        case WM_SIZE:self->release();InvalidateRect(h,nullptr,FALSE);return 0;
        case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);if(!self->direct()){self->lastDirect=false;self->fallback(dc);}EndPaint(h,&ps);return 0;}
        // PrintWindow requests an off-screen compatible paint; it does not certify a GPU.
        case WM_PRINTCLIENT:self->fallback(reinterpret_cast<HDC>(w));return 0;
        case WM_LBUTTONDOWN:if(self->mode==1){SetFocus(h);SetCapture(h);self->pointer(l);}return 0;
        case WM_MOUSEMOVE:if(GetCapture()==h)self->pointer(l);return 0;
        case WM_LBUTTONUP:if(GetCapture()==h)ReleaseCapture();return 0;
        case WM_KEYDOWN:if(self->mode==1&&(w==VK_LEFT||w==VK_RIGHT||w==VK_HOME||w==VK_END)){
            self->split=w==VK_HOME?0.f:w==VK_END?1.f:wipe(self->split+(w==VK_LEFT?-.05f:.05f));self->announce();InvalidateRect(h,nullptr,FALSE);return 0;}break;
        case WM_GETDLGCODE:return DLGC_WANTARROWS;
        case WM_DESTROY:self->release();self->hwnd=nullptr;return 0;
        }return DefWindowProcW(h,m,w,l);
    }
public:
    HWND hwnd=nullptr;int mode=0;bool actual=false;float split=.5f;
    Presenter()=default;Presenter(const Presenter&)=delete;Presenter& operator=(const Presenter&)=delete;
    ~Presenter(){if(hwnd&&IsWindow(hwnd))DestroyWindow(hwnd);}
    void create(HWND owner,int id){WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_SIZEWE);wc.lpszClassName=L"ShinyComparisonCanvas";RegisterClassW(&wc);
        hwnd=CreateWindowExW(0,wc.lpszClassName,L"Matched source and processed image; wipe uses left and right arrow keys",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,1,1,owner,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),wc.hInstance,this);
        if(!hwnd)throw std::runtime_error("comparison-canvas-unavailable");}
    void frame(unsigned w,unsigned h,std::span<const std::uint8_t> source,std::span<const std::uint8_t> result){
        if(!w||!h||w>960||h>960||std::uint64_t(w)*h>518400||source.size()!=std::uint64_t(w)*h*4||result.size()!=source.size())throw std::runtime_error("invalid-presentation-frame");
        auto left=premultiply(source),right=premultiply(result);before=std::move(left);after=std::move(right);width=w;height=h;dirty=true;InvalidateRect(hwnd,nullptr,FALSE);
    }
    void clear(){before.clear();after.clear();width=height=0;release();if(hwnd)InvalidateRect(hwnd,nullptr,FALSE);}
    void view(int value){mode=std::clamp(value,0,3);announce();InvalidateRect(hwnd,nullptr,FALSE);}
    void software(bool value){compatibility=value;release();InvalidateRect(hwnd,nullptr,FALSE);}
    const wchar_t* renderer()const{return lastDirect?L"Direct2D presentation":L"GDI+ compatibility presentation";}
};
}
