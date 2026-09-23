// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <windows.h>
#include <vlc/vlc.h>
#include <filesystem>
#include <memory>
#include <utility>

namespace shiny::player {
std::string utf8(const std::wstring& value);
std::wstring wide(const std::string& value);
std::filesystem::path installedVlc();
#define VLC_SYMBOLS(X) \
 X(New, libvlc_new) X(Release, libvlc_release) X(Version, libvlc_get_version) X(Free, libvlc_free) \
 X(NewPath, libvlc_media_new_path) X(NewLocation, libvlc_media_new_location) X(ReleaseMedia, libvlc_media_release) \
 X(MediaOption, libvlc_media_add_option) X(GetMrl, libvlc_media_get_mrl) X(GetStats, libvlc_media_get_stats) \
 X(NewPlayer, libvlc_media_player_new) X(ReleasePlayer, libvlc_media_player_release) X(SetMedia, libvlc_media_player_set_media) \
 X(SetHwnd, libvlc_media_player_set_hwnd) X(Play, libvlc_media_player_play) X(Pause, libvlc_media_player_set_pause) \
 X(Stop, libvlc_media_player_stop) X(State, libvlc_media_player_get_state) X(Time, libvlc_media_player_get_time) \
 X(Length, libvlc_media_player_get_length) X(SetTime, libvlc_media_player_set_time) X(Seekable, libvlc_media_player_is_seekable) \
 X(SetRate, libvlc_media_player_set_rate) X(GetRate, libvlc_media_player_get_rate) X(NextFrame, libvlc_media_player_next_frame) \
 X(SetVolume, libvlc_audio_set_volume) X(GetVolume, libvlc_audio_get_volume) X(SetMute, libvlc_audio_set_mute) \
 X(SetAudioDelay, libvlc_audio_set_delay) X(SetSubtitleDelay, libvlc_video_set_spu_delay) \
 X(GetAudioTracks, libvlc_audio_get_track_description) X(GetSubtitles, libvlc_video_get_spu_description) \
 X(SetAudioTrack, libvlc_audio_set_track) X(GetAudioTrack, libvlc_audio_get_track) \
 X(SetSubtitle, libvlc_video_set_spu) X(GetSubtitle, libvlc_video_get_spu) X(FreeTracks, libvlc_track_description_list_release) \
 X(AddSlave, libvlc_media_player_add_slave) X(SetAspect, libvlc_video_set_aspect_ratio) X(SetCrop, libvlc_video_set_crop_geometry) \
 X(SetDeinterlace, libvlc_video_set_deinterlace) X(Snapshot, libvlc_video_take_snapshot) X(VideoSize, libvlc_video_get_size) \
 X(HasVideo, libvlc_media_player_has_vout) X(SetAdjustInt, libvlc_video_set_adjust_int) X(SetAdjust, libvlc_video_set_adjust_float) \
 X(GetAdjustInt, libvlc_video_get_adjust_int) X(GetAdjust, libvlc_video_get_adjust_float) \
 X(NextChapter, libvlc_media_player_next_chapter) X(PreviousChapter, libvlc_media_player_previous_chapter) \
 X(GetChapter, libvlc_media_player_get_chapter) X(ChapterCount, libvlc_media_player_get_chapter_count) \
 X(SetChapter, libvlc_media_player_set_chapter) X(SetKeys, libvlc_video_set_key_input) X(SetMouse, libvlc_video_set_mouse_input) \
 X(EqCount, libvlc_audio_equalizer_get_preset_count) X(EqName, libvlc_audio_equalizer_get_preset_name) \
 X(EqPreset, libvlc_audio_equalizer_new_from_preset) X(EqRelease, libvlc_audio_equalizer_release) X(SetEq, libvlc_media_player_set_equalizer) \
 X(SetCallbacks, libvlc_video_set_callbacks) X(SetFormat, libvlc_video_set_format)
class VlcApi {
 public:
  std::filesystem::path root;
  std::string runtimeVersion;
  static std::shared_ptr<VlcApi> load(const std::filesystem::path& folder);
  ~VlcApi();
  VlcApi(const VlcApi&) = delete; VlcApi& operator=(const VlcApi&) = delete;
#define FIELD(field, symbol) decltype(&symbol) field = nullptr;
  VLC_SYMBOLS(FIELD)
#undef FIELD
 private:
  VlcApi()=default;
  HMODULE module=nullptr, core=nullptr;
  DLL_DIRECTORY_COOKIE directory=nullptr;
};
class Engine {
 public:
  std::shared_ptr<VlcApi> api;
  libvlc_instance_t* instance=nullptr;
  libvlc_media_player_t* player=nullptr;
  libvlc_media_t* media=nullptr;
  Effects effects;
  bool driverSuperResolutionRequested=false;
  explicit Engine(std::shared_ptr<VlcApi> runtime, HWND surface, bool silent=false, bool test=false, bool driverSuper=false);
  ~Engine();
  Engine(const Engine&)=delete; Engine& operator=(const Engine&)=delete;
  void open(const Item& item);
  void stop();
  void pause(bool value);
  bool seek(int64_t milliseconds);
  void applyEffects();
  void equalizer(int index);
  void subtitle(const std::filesystem::path& file);
  std::vector<std::pair<int,std::wstring>> tracks(bool audio) const;
  bool snapshot(const std::filesystem::path& file) const;
  bool playing() const {return api->State(player)==libvlc_Playing;}
  int64_t time() const {return api->Time(player);}
  int64_t length() const {return api->Length(player);}
  bool seekable() const {return api->Seekable(player)!=0;}
 private:
  bool silent=false;
};
int playbackTest(const std::filesystem::path& root,const std::filesystem::path& fixture,const std::filesystem::path& report);
}
