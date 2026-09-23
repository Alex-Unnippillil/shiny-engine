// SPDX-License-Identifier: MIT
#include "ui.hpp"
namespace shiny::ui {

void Window::loadRuntime(const std::filesystem::path& folder){
  if(api)throw std::runtime_error("Restart the player to change VLC installations.");
  auto next=VlcApi::load(folder);auto nextEngine=std::make_unique<Engine>(next,video);
  engine=std::move(nextEngine);api=std::move(next);message(L"Ready · libVLC "+wide(api->runtimeVersion)+L" · local processing · DLSS 5 unavailable");
 }

void Window::addFiles(const std::vector<std::filesystem::path>& files){
  size_t first=queue.items.size();
  for(auto& p:files){auto full=std::filesystem::absolute(p);if(!std::filesystem::is_regular_file(full))continue;queue.add({full.wstring(),full.filename().wstring(),false});SendMessageW(queueBox,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(queue.items.back().title.c_str()));}
  if(queue.items.size()>first)select(first);
 }

void Window::closeComparison(){if(treatment){treatment.reset();syncPending=false;layout();if(engine){engine->effects=adjustments;engine->applyEffects();}}}

void Window::select(size_t index){
  if(!engine)throw std::runtime_error("Install or locate VLC first.");
  if(index>=queue.items.size())return;
  closeComparison();if(engine->driverSuperResolutionRequested!=driverSuper){auto replacement=std::make_unique<Engine>(api,video,false,false,driverSuper);engine=std::move(replacement);}engine->open(queue.items[index]);queue.choose(index);loop.clear();endHandled=false;fxApplied=false;
  engine->api->SetRate(engine->player,playbackRate);engine->api->SetVolume(engine->player,volume);engine->api->SetMute(engine->player,mutedAudio?1:0);engine->equalizer(eqIndex);
  SendMessageW(queueBox,LB_SETCURSEL,index,0);SetWindowTextW(hwnd,(L"Shiny Player — "+queue.items[index].title).c_str());message(L"Opening media with VLC...");
 }

void Window::playPause(){if(!engine)return;bool pause=engine->playing();auto st=api->State(engine->player);if(st==libvlc_Stopped||st==libvlc_Ended||st==libvlc_NothingSpecial){if(queue.selected)select(*queue.selected);else if(!queue.items.empty())select(0);return;}engine->pause(pause);if(treatment)treatment->pause(pause);}

void Window::seek(int64_t t){if(!engine)return;if(!engine->seek(t)){message(L"This source is not seekable.");return;}if(treatment)treatment->seek(t);}

void Window::fullscreenToggle(){
  if(!fullscreen){placement.length=sizeof(placement);GetWindowPlacement(hwnd,&placement);oldStyle=GetWindowLongPtrW(hwnd,GWL_STYLE);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);SetMenu(hwnd,nullptr);SetWindowLongPtrW(hwnd,GWL_STYLE,oldStyle&~WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);fullscreen=true;}
  else{SetWindowLongPtrW(hwnd,GWL_STYLE,oldStyle);SetMenu(hwnd,menu);SetWindowPlacement(hwnd,&placement);SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);fullscreen=false;}layout();
 }

void Window::fullVlc(){
  if(!api)throw std::runtime_error("Locate an official VLC installation first.");
  const auto exe=(api->root/L"vlc.exe").wstring();std::wstring command=quote(exe)+L" --no-one-instance";
  if(queue.selected){auto& item=queue.items[*queue.selected];if(!item.remote && engine && engine->time()>0)command+=L" --start-time="+std::to_wstring(engine->time()/1000);command+=L" -- "+quote(item.source);}
  STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
  if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,api->root.c_str(),&si,&pi))throw std::runtime_error("Could not launch the installed VLC interface.");
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);if(engine)engine->pause(true);if(treatment)treatment->pause(true);message(L"Full VLC opened separately. Shiny playback paused to prevent duplicate audio.");
 }

void Window::refreshMenu(HMENU m){
  auto clear=[&]{while(GetMenuItemCount(m)>0)DeleteMenu(m,0,MF_BYPOSITION);};
  if(m==audioMenu||m==subMenu){clear();auto& tracks=m==audioMenu?audioTracks:subTracks;tracks=engine?engine->tracks(m==audioMenu):std::vector<std::pair<int,std::wstring>>{};
   int current=engine?(m==audioMenu?api->GetAudioTrack(engine->player):api->GetSubtitle(engine->player)):-99;
   for(size_t i=0;i<std::min<size_t>(999,tracks.size());++i)AppendMenuW(m,MF_STRING|(tracks[i].first==current?MF_CHECKED:0),(m==audioMenu?AUDIOFIRST:SUBFIRST)+i,tracks[i].second.c_str());
   if(tracks.empty())AppendMenuW(m,MF_STRING|MF_GRAYED,0,L"No tracks available yet");
  }else if(m==eqMenu){clear();AppendMenuW(m,MF_STRING|(eqIndex<0?MF_CHECKED:0),EQFIRST,L"Off");if(api)for(unsigned i=0;i<std::min<unsigned>(998,api->EqCount());++i)AppendMenuW(m,MF_STRING|(eqIndex==static_cast<int>(i)?MF_CHECKED:0),EQFIRST+1+i,wide(api->EqName(i)).c_str());
  }else if(m==chapterMenu){clear();int n=engine?api->ChapterCount(engine->player):0;for(int i=0;i<std::min(n,999);++i)AppendMenuW(m,MF_STRING|(api->GetChapter(engine->player)==i?MF_CHECKED:0),CHAPTERFIRST+i,(L"Chapter "+std::to_wstring(i+1)).c_str());if(n<=0)AppendMenuW(m,MF_STRING|MF_GRAYED,0,L"No chapters in this source");}
 }

void Window::effectsFromControls(){
  adjustments.enabled=SendMessageW(GetDlgItem(hwnd,FXENABLE),BM_GETCHECK,0,0)==BST_CHECKED;
  adjustments.contrast=static_cast<float>(SendMessageW(GetDlgItem(hwnd,CONTRAST),TBM_GETPOS,0,0))/100;
  adjustments.brightness=static_cast<float>(SendMessageW(GetDlgItem(hwnd,BRIGHTNESS),TBM_GETPOS,0,0))/100;
  adjustments.saturation=static_cast<float>(SendMessageW(GetDlgItem(hwnd,SATURATION),TBM_GETPOS,0,0))/100;
  adjustments.gamma=static_cast<float>(SendMessageW(GetDlgItem(hwnd,GAMMA),TBM_GETPOS,0,0))/100;
  if(engine){engine->effects=adjustments;if(treatment)engine->effects.enabled=false;engine->applyEffects();}
  message(treatment?L"Comparison preserves the source. Adjustments resume after closing comparison.":L"Live VLC image adjustments. These are conventional filters, not DLSS or neural rendering.");
 }

void Window::diagnostics(const std::filesystem::path& path){
  unsigned width=0,height=0;libvlc_media_stats_t stats{};if(engine){api->VideoSize(engine->player,0,&width,&height);if(engine->media)api->GetStats(engine->media,&stats);}
  std::ofstream out(path);out<<"{\n  \"schema\":1,\n  \"app\":\"0.5.0\",\n  \"runtime\":\""<<(api?api->runtimeVersion:"not loaded")<<"\",\n  \"width\":"<<width<<",\n  \"height\":"<<height<<",\n  \"decodedVideoFrames\":"<<stats.i_decoded_video<<",\n  \"displayedFrames\":"<<stats.i_displayed_pictures<<",\n  \"lostPictures\":"<<stats.i_lost_pictures<<",\n  \"importedComparison\":"<<(treatment?"true":"false")<<",\n  \"driverSuperResolutionRequested\":"<<(engine&&engine->driverSuperResolutionRequested?"true":"false")<<",\n  \"driverSuperResolutionVerified\":false,\n  \"dlss5Inference\":false,\n  \"limitation\":\"No model inference, hardware speed certification or frame-exact dual-player sync. No paths or URLs included.\"\n}\n";
  if(!out)throw std::runtime_error("Could not save diagnostics.");
 }

void Window::tick(){
  if(engine){auto st=api->State(engine->player);SetWindowTextW(GetDlgItem(hwnd,PLAY),st==libvlc_Playing?L"Pause":L"Play");
   bool canSeek=engine->seekable();EnableWindow(seekBar,canSeek);if(!seeking){auto len=engine->length(),t=engine->time();SendMessageW(seekBar,TBM_SETPOS,TRUE,len>0?static_cast<LPARAM>(std::clamp<int64_t>(t*10000/len,0,10000)):0);SetWindowTextW(GetDlgItem(hwnd,303),(clock(t)+L" / "+clock(len)+(loop.b?L"   ·   A-B loop":L"")).c_str());}
   if(!fxApplied&&api->HasVideo(engine->player)){engine->effects=adjustments;if(treatment)engine->effects.enabled=false;engine->applyEffects();fxApplied=true;message(L"Playing with libVLC "+wide(api->runtimeVersion)+(engine->driverSuperResolutionRequested?L" · VSR REQUESTED, hardware unverified · not DLSS 5":L" · DLSS 5 inference unavailable"));}
   if(auto target=loop.target(engine->time(),engine->playing(),canSeek))seek(*target);
   if(treatment && GetTickCount64()-lastSync>500){lastSync=GetTickCount64();auto ts=api->State(treatment->player);
    if(ts==libvlc_Error||ts==libvlc_Ended){closeComparison();message(L"Imported comparison ended or could not decode; source playback is preserved.");}
    else if(ts==libvlc_Playing||ts==libvlc_Paused){
     if(engine->length()>0&&treatment->length()>0&&std::llabs(engine->length()-treatment->length())>1500){closeComparison();message(L"Comparison rejected: video durations differ by more than 1.5 seconds.");}
     else{if(syncPending||std::llabs(engine->time()-treatment->time())>120)treatment->seek(engine->time());treatment->pause(!engine->playing());syncPending=false;}
    }
   }
   if(st==libvlc_Ended&&!endHandled){endHandled=true;auto n=queue.next(true);if(n)select(*n);else message(L"Playback finished.");}
   if(st==libvlc_Error&&!endHandled){endHandled=true;closeComparison();message(L"VLC could not decode this source. Choose another file or use Full VLC for advanced setup.");}
  }
  if(smoke&&GetTickCount64()-started>5000){KillTimer(hwnd,1);try{diagnostics(smokeImage.parent_path()/L"ui-playback.json");if(!engine||!api->HasVideo(engine->player)||!saveWindow(hwnd,smokeImage))smokeExit=1;}catch(...){smokeExit=1;}PostMessageW(hwnd,WM_CLOSE,0,0);}
 }

bool Window::key(MSG& msg){
  if(msg.message!=WM_KEYDOWN)return false;
  if(msg.wParam==VK_ESCAPE){if(fullscreen)fullscreenToggle();else action(STOP);return true;}
  wchar_t cls[64]{};GetClassNameW(msg.hwnd,cls,64);if(std::wstring(cls)==L"Edit"||std::wstring(cls)==L"ComboBox"||std::wstring(cls)==TRACKBAR_CLASSW)return false;
  if(std::wstring(cls)==L"Button"&&msg.wParam==VK_SPACE)return false;
  bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
  int id=0;if(ctrl&&msg.wParam=='O')id=OPEN;else if(ctrl&&msg.wParam=='N')id=NETWORK;else if(ctrl)return false;
  else switch(msg.wParam){case VK_SPACE:id=PLAY;break;case 'S':id=STOP;break;case 'E':id=FRAME;break;case 'F':id=FULLSCREEN;break;case 'M':id=MUTE;break;case 'N':id=NEXT;break;case 'P':id=PREV;break;case VK_OEM_4:id=LOOPA;break;case VK_OEM_6:id=LOOPB;break;
   case VK_LEFT:case VK_RIGHT:if(engine)seek(engine->time()+(msg.wParam==VK_LEFT?-1:1)*((GetKeyState(VK_SHIFT)&0x8000)?30000:5000));return true;}
  if(id){action(id);return true;}return false;
 }

Window::~Window(){treatment.reset();engine.reset();api.reset();if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);}
}
