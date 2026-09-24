// SPDX-License-Identifier: MIT
#pragma once
#include "engine.hpp"
#include "layout.hpp"
#include "nr_client.hpp"
#include "managed_preview.hpp"
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <fstream>
#include <sstream>
#include <array>
namespace shiny::ui {
using namespace shiny::player;
inline constexpr COLORREF bg=RGB(16,20,28),panel=RGB(26,33,45),text=RGB(231,238,247),muted=RGB(151,165,184),accent=RGB(86,218,187);
inline constexpr int OPEN=101,NETWORK=102,LOCATE=103,FULLVLC=104,COMPARE=105,CLOSECOMPARE=106,SNAPSHOT=107,DIAGNOSTICS=108,FULLSCREEN=109,
 PLAY=110,STOP=111,PREV=112,NEXT=113,MUTE=114,REPEAT=115,SHUFFLE=116,LOOPA=117,LOOPB=118,LOOPCLEAR=119,
 SUBFILE=120,AUDIODELAY=121,SUBDELAY=122,FRAME=123,ABOUT=124,DLSS=125,CLEAR=126,REMOVE=127,SAVEQUEUE=128,CHAPTERPREV=129,CHAPTERNEXT=130,VSR=131,CINEMA=132,TOPMOST=133,JUMP=134,BOOKMARK=135,MEDIAINFO=136,NR=137,QUEUEUP=138,QUEUEDOWN=139,LIBRARIES=140,MANAGEDPREVIEW=141,
 SEEK=201,VOLUME=202,RATE=203,QUEUE=204,FXENABLE=205,CONTRAST=206,BRIGHTNESS=207,SATURATION=208,GAMMA=209,RESETFX=210,
 AUDIOFIRST=4000,SUBFIRST=5000,EQFIRST=6000,CHAPTERFIRST=7000,ASPECTFIRST=8000,CROPFIRST=8100,DEINTFIRST=8200,BOOKMARKFIRST=9000,DEVICEFIRST=10000,QUEUEFIRST=11000;
inline const wchar_t* aspects[]={L"Source ratio",L"16:9",L"4:3",L"21:9",L"1:1"};
inline const wchar_t* deinterlace[]={L"Off",L"Automatic / Yadif",L"Blend",L"Bob"};
inline HBRUSH backgroundBrush=nullptr,panelBrush=nullptr;
struct Input {HWND edit=nullptr;bool done=false;std::optional<std::wstring> result;std::wstring label,initial;};
LRESULT CALLBACK inputProc(HWND,UINT,WPARAM,LPARAM);
std::optional<std::wstring> input(HWND,const wchar_t*,const wchar_t*,const wchar_t* initial=L"");
std::vector<std::filesystem::path> pick(HWND,bool multiple=false,bool folder=false,bool save=false,const wchar_t* ext=L"png");
bool saveWindow(HWND,const std::filesystem::path&);
LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);
struct Window {
 HWND hwnd=nullptr,video=nullptr,treatmentVideo=nullptr,queueBox=nullptr,seekBar=nullptr,volumeBar=nullptr,rateBox=nullptr,status=nullptr,primaryLabel=nullptr,secondaryLabel=nullptr;
 UINT dpi=96;HFONT font=nullptr,titleFont=nullptr; HMENU audioMenu=nullptr,subMenu=nullptr,eqMenu=nullptr,chapterMenu=nullptr,bookmarkMenu=nullptr,deviceMenu=nullptr,queueMenu=nullptr;
 std::vector<HWND> controls;std::vector<std::pair<int,std::wstring>> audioTracks,subTracks;
 std::vector<int64_t> bookmarks;std::vector<std::string> audioDevices;std::unique_ptr<NeuralPanel> neural;std::unique_ptr<ManagedPanel> managed;bool cinema=false,topmost=false;
 std::shared_ptr<VlcApi> api;std::unique_ptr<Engine> engine,treatment;
 Queue queue;LoopRange loop;Effects adjustments;std::filesystem::path runtimeFolder;
 bool fullscreen=false,mutedAudio=false,seeking=false,endHandled=false,fxApplied=false,syncPending=false;
 bool driverSuper=false;
 int volume=80;float playbackRate=1;int eqIndex=-1;int64_t audioDelay=0,subtitleDelay=0;
 ULONGLONG lastSync=0,started=0;WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};LONG_PTR oldStyle=0;HMENU menu=nullptr;
 std::filesystem::path smokeImage;bool smoke=false;int smokeExit=0;
 HWND control(const wchar_t*,const wchar_t*,int,DWORD extra=0);
 HWND button(const wchar_t*,int);HWND label(const wchar_t*,int);
 int units(int value)const{return MulDiv(value,static_cast<int>(dpi),96);}
 void applyDpi(UINT);void move(int,int,int,int,int);void message(const std::wstring&);void failure(const std::exception&);
 void createMenus();void create();void loadRuntime(const std::filesystem::path&);void layout();
 void addFiles(const std::vector<std::filesystem::path>&);void closeComparison();void select(size_t);
 void playPause();void seek(int64_t);void fullscreenToggle();void fullVlc();void refreshMenu(HMENU);
 void effectsFromControls();void diagnostics(const std::filesystem::path&);void action(int);void tick();
 void drawButton(DRAWITEMSTRUCT*);bool key(MSG&);~Window();
};
}
