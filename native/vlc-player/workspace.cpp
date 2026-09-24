// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include "quick_actions.hpp"
#include "workspace_model.hpp"
namespace shiny::ui {
std::optional<std::size_t> Window::queueIndex() const {
    return mappedQueueIndex(visibleQueue,SendMessageW(queueBox,LB_GETCURSEL,0,0),queue.items.size());
}
void Window::refreshQueue(std::optional<std::size_t> preferred) {
    if(!preferred)preferred=queueIndex();
    wchar_t filter[257]{};GetWindowTextW(GetDlgItem(hwnd,QUEUESEARCH),filter,257);queueFilter=filter;
    visibleQueue=queueMatches(queue,queueFilter);
    SendMessageW(queueBox,WM_SETREDRAW,FALSE,0);SendMessageW(queueBox,LB_RESETCONTENT,0,0);
    int selection=-1;
    for(std::size_t i=0;i<visibleQueue.size();++i){
        auto at=visibleQueue[i];const auto& item=queue.items[at];
        auto label=item.title+(queue.selected==at?L" — current source":L" — queued")+(item.remote?L" · network":L" · local");
        SendMessageW(queueBox,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        if(preferred==at)selection=static_cast<int>(i);
    }
    if(selection<0&&!visibleQueue.empty())selection=0;
    SendMessageW(queueBox,LB_SETCURSEL,selection,0);SendMessageW(queueBox,WM_SETREDRAW,TRUE,0);
    InvalidateRect(queueBox,nullptr,TRUE);
    auto count=std::to_wstring(visibleQueue.size())+L" / "+std::to_wstring(queue.items.size())+L" items · double-click or Enter to play";
    if(visibleQueue.empty())count=queue.items.empty()?L"Your queue is empty. Open or drop media.":L"No matching titles. Clear the search to see all.";
    SetWindowTextW(GetDlgItem(hwnd,QUEUECOUNT),count.c_str());
    updateQueueControls();
}
void Window::updateQueueControls(){
    auto selected=queueIndex();
    EnableWindow(GetDlgItem(hwnd,REMOVE),selected.has_value());EnableWindow(GetDlgItem(hwnd,CLEAR),!queue.items.empty());
    EnableWindow(GetDlgItem(hwnd,QUEUEUP),selected&&*selected>0&&queueFilter.empty());
    EnableWindow(GetDlgItem(hwnd,QUEUEDOWN),selected&&*selected+1<queue.items.size()&&queueFilter.empty());
}
void Window::drawQueue(const DRAWITEMSTRUCT& d) {
    FillRect(d.hDC,&d.rcItem,theme.card);
    if(d.itemID>=visibleQueue.size())return;
    const auto index=visibleQueue[d.itemID];if(index>=queue.items.size())return;
    const auto& item=queue.items[index];auto box=d.rcItem;InflateRect(&box,-units(4),-units(4));
    const bool focus=(d.itemState&ODS_SELECTED)!=0,current=queue.selected==index;
    theme.rounded(d.hDC,box,theme.surface,focus?theme.accent:theme.line,12);
    RECT title{box.left+units(12),box.top+units(7),box.right-units(12),box.top+units(28)};
    theme.text(d.hDC,item.title,title,font,theme.ink);
    auto detail=(current?L"CURRENT SOURCE":L"QUEUED")+std::wstring(item.remote?L"  ·  Network":L"  ·  Local file");
    theme.text(d.hDC,detail,{title.left,title.bottom,title.right,box.bottom-units(4)},theme.captionFont,current?theme.accent:theme.muted);
    if(d.itemState&ODS_FOCUS){InflateRect(&box,-units(2),-units(2));DrawFocusRect(d.hDC,&box);}
}
void Window::showCommands() {
    const bool loaded=engine&&queue.selected.has_value();
    const bool local=loaded&&!queue.items[*queue.selected].remote&&engine->seekable()&&api->HasVideo(engine->player);
    std::vector<QuickAction> commands{
        {OPEN,L"Open media",L"Choose local files",L"Ctrl+O"},
        {NETWORK,L"Open network stream",L"Enter an explicit media URL",L"Ctrl+N"},
        {PLAY,L"Play or pause",L"Control primary playback",L"Space",loaded},
        {STOP,L"Stop playback",L"Stop playback and close independent previews",L"S",loaded},
        {MANAGEDPREVIEW,L"Video studio",L"Compare local spatial filters; not DLSS",L"",local},
        {LIBRARIES,L"Enhancement libraries",L"Import, verify, stage and select packages",L""},
        {CINEMA,L"Toggle cinema view",L"Hide or restore the workspace sidebar",L"C"},
        {SHOWQUEUE,L"Queue workspace",L"Find and manage queued media",L"Ctrl+F"},
        {SHOWFX,L"Live adjustments",L"Contrast, brightness, saturation and gamma",L"",loaded},
        {BOOKMARK,L"Bookmark current time",L"Keep a session-only time marker",L"B",loaded},
        {JUMP,L"Go to time",L"Seek by seconds or HH:MM:SS",L"J",loaded},
        {NR,L"Neural research",L"Validated authorized local models; experimental",L"",local},
        {FULLSCREEN,L"Toggle fullscreen",L"Expand or restore the player",L"F"},
        {LOCATE,L"Locate VLC runtime",L"Choose an existing official VLC folder",L"",!api},
        {ABOUT,L"Keyboard shortcuts",L"Help and application information",L"F1"}
    };
    if(auto command=quickActions(hwnd,commands))action(*command);
}
void Window::updateWorkspace() {
    const bool loaded=engine&&queue.selected.has_value();
    const bool local=loaded&&!queue.items[*queue.selected].remote&&engine->seekable()&&api->HasVideo(engine->player);
    auto st=engine?api->State(engine->player):libvlc_NothingSpecial;
    std::wstring badge=!engine?L"VLC REQUIRED":st==libvlc_Playing?L"PLAYING":st==libvlc_Paused?L"PAUSED":st==libvlc_Error?L"SOURCE ERROR":st==libvlc_Opening||st==libvlc_Buffering?L"OPENING":L"READY";
    if(badge!=workspaceBadge){workspaceBadge=badge;SetWindowTextW(GetDlgItem(hwnd,STATEBADGE),badge.c_str());}
    const auto title=queue.selected?L"CURRENT SOURCE  /  "+queue.items[*queue.selected].title:L"YOUR VIEWING SPACE";
    if(title!=workspaceTitle){workspaceTitle=title;SetWindowTextW(primaryLabel,title.c_str());}
    EnableWindow(GetDlgItem(hwnd,PLAY),loaded);EnableWindow(GetDlgItem(hwnd,STOP),loaded);
    for(int id:{PREV,NEXT})EnableWindow(GetDlgItem(hwnd,id),loaded&&queue.items.size()>1);
    for(int id:{NR,MANAGEDPREVIEW})EnableWindow(GetDlgItem(hwnd,id),local);
    EnableWindow(GetDlgItem(hwnd,SNAPSHOT),loaded&&api->HasVideo(engine->player));
    SetWindowTextW(GetDlgItem(hwnd,WELCOMEOPEN),engine?L"Open media":L"Locate VLC");
    SetWindowTextW(GetDlgItem(hwnd,WELCOMETITLE),engine?L"Your next watch starts here":L"Connect your VLC runtime");
    SetWindowTextW(GetDlgItem(hwnd,WELCOMEBODY),engine?L"Drop local files here, or open an explicit stream.\nYour media stays on your computer.":L"Select an official 64-bit VLC 3.0.24+ folder.\nThe Windows installer can fetch the verified runtime.");
    EnableWindow(GetDlgItem(hwnd,WELCOMENET),engine!=nullptr);
    const bool empty=!queue.selected;
    if(empty!=workspaceEmpty){workspaceEmpty=empty;layout();}
}
void Window::paintWorkspace(HDC dc) {
    RECT client{};GetClientRect(hwnd,&client);auto p=[&](int n){return units(n);};
    theme.rounded(dc,{p(20),p(17),p(60),p(57)},theme.accent,theme.accent,12);
    POINT triangle[]={{p(35),p(28)},{p(35),p(46)},{p(48),p(37)}};
    auto brush=CreateSolidBrush(theme.bg);auto old=SelectObject(dc,brush);Polygon(dc,triangle,3);SelectObject(dc,old);DeleteObject(brush);
    theme.text(dc,L"Shiny Player",{p(74),p(15),p(285),p(44)},titleFont,theme.ink);
    theme.text(dc,L"PLAY · REVIEW · EXPLORE",{p(75),p(47),p(340),p(65)},theme.captionFont,theme.muted);
    RECT badge{client.right-p(338),p(23),client.right-p(198),p(55)};
    theme.rounded(dc,badge,theme.surface,theme.line,18);
    auto layout=workspaceLayout(MulDiv(client.right,96,dpi),MulDiv(client.bottom,96,dpi),cinema,treatment!=nullptr);
    if(workspaceEmpty){auto b=layout.original;theme.rounded(dc,{p(b.x),p(b.y),p(b.x+b.w),p(b.y+b.h)},theme.surface,theme.line,18);}
    RECT tray{p(12),p(layout.time.y-6),client.right-p(12),p(layout.status.y-6)};
    theme.rounded(dc,tray,theme.surface,theme.line,14);
}
LRESULT Window::drawSlider(NMCUSTOMDRAW* d) {
    if(theme.contrast)return CDRF_DODEFAULT;
    if(d->dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
    if(d->dwDrawStage!=CDDS_ITEMPREPAINT)return CDRF_DODEFAULT;
    if(d->dwItemSpec==TBCD_CHANNEL){
        RECT r=d->rc;auto middle=(r.top+r.bottom)/2;r.top=middle-units(2);r.bottom=middle+units(2);
        auto brush=CreateSolidBrush(theme.line);FillRect(d->hdc,&r,brush);DeleteObject(brush);
        auto c=d->hdr.hwndFrom;auto low=SendMessageW(c,TBM_GETRANGEMIN,0,0),high=SendMessageW(c,TBM_GETRANGEMAX,0,0),at=SendMessageW(c,TBM_GETPOS,0,0);
        if(high>low){r.right=r.left+static_cast<LONG>((r.right-r.left)*(at-low)/(high-low));brush=CreateSolidBrush(IsWindowEnabled(c)?theme.accent:theme.muted);FillRect(d->hdc,&r,brush);DeleteObject(brush);}return CDRF_SKIPDEFAULT;
    }
    if(d->dwItemSpec==TBCD_THUMB){theme.rounded(d->hdc,d->rc,IsWindowEnabled(d->hdr.hwndFrom)?theme.accent:theme.muted,GetFocus()==d->hdr.hwndFrom?theme.ink:theme.accent,8);return CDRF_SKIPDEFAULT;}
    return CDRF_DODEFAULT;
}
}
