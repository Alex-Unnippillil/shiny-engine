// SPDX-License-Identifier: MIT
#include "ui.hpp"
namespace shiny::ui {

void Window::action(int id){
  if(id==OPEN){addFiles(pick(hwnd,true));return;}
  if(id==LOCATE){auto files=pick(hwnd,false,true);if(!files.empty())loadRuntime(files[0]);return;}
  if(id==NETWORK){auto s=input(hwnd,L"Open network media",L"Explicit HTTP(S), RTSP, RTP, UDP or SRT media URL. No page scraping.");if(s){if(!network(*s))throw std::runtime_error("Enter a network URL, not a local path.");queue.add({*s,L"Network stream",true});SendMessageW(queueBox,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Network stream"));select(queue.items.size()-1);}return;}
  if(id==FULLSCREEN){fullscreenToggle();return;}
  if(id==VSR){driverSuper=!driverSuper;CheckMenuItem(menu,VSR,MF_BYCOMMAND|(driverSuper?MF_CHECKED:MF_UNCHECKED));message(driverSuper?L"Driver super resolution requested for the NEXT media open. Uses VLC D3D11; hardware success unverified. Not DLSS 5.":L"Automatic VLC scaling selected for the NEXT media open.");return;}
  if(id==DLSS){MessageBoxW(hwnd,L"DLSS 5 neural rendering is NOT enabled in this release.\n\nThis native player uses VLC/libVLC for playback and conventional live image adjustments. It does not contain a native DLSS 5 inference adapter, approved model, or NVIDIA runtime.\n\nYou can load a video processed by your separately installed renderer and compare it against the source. Imported provenance is unverified. The existing experimental browser Neural Lab remains separately gated.\n\nNo leaked DLLs or models are downloaded. The separate Video menu can request VLC driver super resolution on the next media open. This can use RTX VSR on supported NVIDIA systems; driver success is not verified by this app. It is NOT DLSS 5.",L"DLSS 5 — unavailable",MB_OK|MB_ICONINFORMATION);return;}
  if(id==ABOUT){MessageBoxW(hwnd,L"Shiny Player 0.5.0 — powered by libVLC\nIndependent of VideoLAN and NVIDIA.\n\nSpace: play/pause  ·  S: stop  ·  E: next frame\nLeft/Right: seek 5 s  ·  Shift+Left/Right: 30 s\nN/P: next/previous  ·  M: mute  ·  F: fullscreen\n[ / ]: set A/B loop  ·  Esc: leave fullscreen or stop\nCtrl+O: files  ·  Ctrl+N: network stream\n\nFull VLC opens the original installed interface for disc/device capture, conversion, casting, advanced filters and other features not duplicated here. The custom player is not full VLC UI parity.\n\nlibVLC uses LGPL-2.1-or-later; VLC is GPL-2.0-or-later (some dependency builds GPL-3.0). See THIRD_PARTY_NOTICES.md. No media history or remote telemetry is stored.",L"About Shiny Player",MB_OK);return;}
  if(!engine)throw std::runtime_error("Install official 64-bit VLC 3.0.24+ in the 3.0 series, then use Media > Locate installed VLC.");
  if(id==PLAY){playPause();return;}
  if(id==STOP){closeComparison();engine->stop();loop.clear();endHandled=true;message(L"Playback stopped.");return;}
  if(id==PREV||id==NEXT){auto n=id==PREV?queue.previous():queue.next();if(n)select(*n);return;}
  if(id==CLEAR){closeComparison();engine->stop();queue.clear();loop.clear();endHandled=true;SendMessageW(queueBox,LB_RESETCONTENT,0,0);SetWindowTextW(hwnd,L"Shiny Player");message(L"Queue cleared. No media history is retained.");return;}
  if(id==REMOVE){auto i=SendMessageW(queueBox,LB_GETCURSEL,0,0);if(i==LB_ERR)return;if(queue.selected==static_cast<size_t>(i)){closeComparison();engine->stop();endHandled=true;loop.clear();}queue.erase(static_cast<size_t>(i));SendMessageW(queueBox,LB_DELETESTRING,i,0);return;}
  if(id==SAVEQUEUE){if(queue.items.empty())throw std::runtime_error("The queue is empty.");if(MessageBoxW(hwnd,L"The exported playlist includes the selected file paths and network URLs, which may contain private information. Save it?",L"Export playlist",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;auto f=pick(hwnd,false,false,true,L"m3u8");if(!f.empty()){std::ofstream out(f[0],std::ios::binary);out<<"#EXTM3U\n";for(auto& item:queue.items)out<<utf8(item.source)<<'\n';if(!out)throw std::runtime_error("Cannot export queue.");}return;}
  if(id==MUTE){mutedAudio=!mutedAudio;api->SetMute(engine->player,mutedAudio);SetWindowTextW(GetDlgItem(hwnd,MUTE),mutedAudio?L"Unmute":L"Mute");return;}
  if(id==REPEAT||id==SHUFFLE){bool& value=id==REPEAT?queue.repeat:queue.shuffle;value=!value;CheckMenuItem(menu,id,MF_BYCOMMAND|(value?MF_CHECKED:MF_UNCHECKED));return;}
  if(id==LOOPA){if(!engine->seekable())throw std::runtime_error("A-B loop requires seekable media.");loop.start(engine->time());message(L"Loop A = "+clock(*loop.a)+L". Choose B later in this source.");return;}
  if(id==LOOPB){loop.end(engine->time());message(L"A-B loop enabled. Playback returns to A at B.");return;}
  if(id==LOOPCLEAR){loop.clear();message(L"A-B loop cleared.");return;}
  if(id==FRAME){engine->pause(true);if(treatment){closeComparison();message(L"Comparison closed: stepping applies to the source player only.");}api->NextFrame(engine->player);return;}
  if(id==FXENABLE){effectsFromControls();return;}
  if(id==RESETFX){adjustments={};SendMessageW(GetDlgItem(hwnd,FXENABLE),BM_SETCHECK,BST_UNCHECKED,0);for(int i=0;i<4;++i)SendMessageW(GetDlgItem(hwnd,CONTRAST+i),TBM_SETPOS,TRUE,100);effectsFromControls();return;}
  if(id==FULLVLC){fullVlc();return;}
  if(id==SUBFILE){auto f=pick(hwnd);if(!f.empty()){engine->subtitle(f[0]);message(L"Subtitle added. Select its track in Subtitles.");}return;}
  if(id==AUDIODELAY||id==SUBDELAY){auto value=input(hwnd,L"Track timing",L"Delay in milliseconds, between -10000 and 10000. Positive delays the track.",L"0");if(value){size_t used=0;int n=std::stoi(*value,&used);if(used!=value->size()||n< -10000||n>10000)throw std::runtime_error("Invalid delay.");int rc=id==AUDIODELAY?api->SetAudioDelay(engine->player,static_cast<int64_t>(n)*1000):api->SetSubtitleDelay(engine->player,static_cast<int64_t>(n)*1000);if(rc<0)throw std::runtime_error("Track timing unavailable for this source.");if(id==AUDIODELAY)audioDelay=n;else subtitleDelay=n;}return;}
  if(id==SNAPSHOT){if(!api->HasVideo(engine->player))throw std::runtime_error("Play a video before taking a snapshot.");auto f=pick(hwnd,false,false,true);if(!f.empty()){if(!engine->snapshot(f[0]))throw std::runtime_error("This VLC output could not save a snapshot.");message(L"Displayed source frame saved. It may include enabled VLC filters/subtitles; it is not a raw source or DLSS result.");}return;}
  if(id==DIAGNOSTICS){auto f=pick(hwnd,false,false,true,L"json");if(!f.empty()){diagnostics(f[0]);message(L"Diagnostics saved without media paths or URLs.");}return;}
  if(id==COMPARE){if(!queue.selected||queue.items[*queue.selected].remote||!engine->seekable())throw std::runtime_error("Open a seekable local source first.");auto f=pick(hwnd);if(f.empty())return;
   if(MessageBoxW(hwnd,L"Choose the same video, timeline and crop processed by your external renderer. This is imported-output playback, not DLSS inference.\n\nThe source supplies the only audio. Synchronization is best-effort and not frame-exact. Continue?",L"Compare imported treatment",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;
   closeComparison();auto next=std::make_unique<Engine>(api,treatmentVideo,true);next->open({f[0].wstring(),L"Imported treatment",false});next->api->SetRate(next->player,playbackRate);treatment=std::move(next);engine->effects.enabled=false;engine->applyEffects();syncPending=true;lastSync=0;layout();message(L"Comparing imported treatment. Source audio only · best-effort sync · no model provenance verification.");return;}
  if(id==CLOSECOMPARE){closeComparison();message(L"Comparison closed.");return;}
  if(id==CHAPTERPREV||id==CHAPTERNEXT){closeComparison();if(id==CHAPTERPREV)api->PreviousChapter(engine->player);else api->NextChapter(engine->player);return;}
  if(id>=AUDIOFIRST&&id<AUDIOFIRST+999){auto i=static_cast<size_t>(id-AUDIOFIRST);if(i<audioTracks.size()&&api->SetAudioTrack(engine->player,audioTracks[i].first)<0)throw std::runtime_error("Could not select audio track.");return;}
  if(id>=SUBFIRST&&id<SUBFIRST+999){auto i=static_cast<size_t>(id-SUBFIRST);if(i<subTracks.size()&&api->SetSubtitle(engine->player,subTracks[i].first)<0)throw std::runtime_error("Could not select subtitle track.");return;}
  if(id>=EQFIRST&&id<EQFIRST+999){engine->equalizer(id-EQFIRST-1);eqIndex=id-EQFIRST-1;return;}
  if(id>=CHAPTERFIRST&&id<CHAPTERFIRST+999){closeComparison();api->SetChapter(engine->player,id-CHAPTERFIRST);return;}
  if(id>=ASPECTFIRST&&id<ASPECTFIRST+5){auto s=id==ASPECTFIRST?std::string{}:utf8(aspects[id-ASPECTFIRST]);api->SetAspect(engine->player,s.empty()?nullptr:s.c_str());if(treatment)api->SetAspect(treatment->player,s.empty()?nullptr:s.c_str());return;}
  if(id>=CROPFIRST&&id<CROPFIRST+5){auto s=id==CROPFIRST?std::string{}:utf8(aspects[id-CROPFIRST]);api->SetCrop(engine->player,s.empty()?nullptr:s.c_str());if(treatment)api->SetCrop(treatment->player,s.empty()?nullptr:s.c_str());return;}
  if(id>=DEINTFIRST&&id<DEINTFIRST+4){const char* modes[]={nullptr,"yadif","blend","bob"};api->SetDeinterlace(engine->player,modes[id-DEINTFIRST]);return;}
 }
}
