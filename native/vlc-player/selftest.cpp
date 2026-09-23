// SPDX-License-Identifier: MIT
// Real libVLC tests and native-worker rejection; no trained-model inference claim.
#include "engine.hpp"
#include "nr_client.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <malloc.h>
namespace shiny::player {
namespace {
struct Frames {
 std::mutex mutex;unsigned char* pixels=static_cast<unsigned char*>(_aligned_malloc(160*96*4,64));std::atomic<unsigned> count{0},changes{0};uint64_t lastHash=0;double mean=0;
 Frames(){if(!pixels)throw std::bad_alloc();}~Frames(){_aligned_free(pixels);}
 static void* lock(void* opaque,void** planes){auto* self=static_cast<Frames*>(opaque);self->mutex.lock();*planes=self->pixels;return nullptr;}
 static void unlock(void* opaque,void*,void* const*){auto* self=static_cast<Frames*>(opaque);double total=0;for(unsigned y=24;y<64;++y)for(unsigned x=30;x<110;++x)for(unsigned c=0;c<3;++c)total+=self->pixels[(y*160+x)*4+c];self->mean=total/(40*80*3);
  uint64_t hash=1469598103934665603ull;for(unsigned i=0;i<160*96*4;++i)if(i%4!=3){hash^=self->pixels[i];hash*=1099511628211ull;}if(hash!=self->lastHash){self->lastHash=hash;++self->changes;}self->mutex.unlock();}
 static void display(void* opaque,void*){++static_cast<Frames*>(opaque)->count;}
 double average(){std::lock_guard guard(mutex);return mean;}
 void attach(Engine& engine){engine.api->SetCallbacks(engine.player,lock,unlock,display,this);engine.api->SetFormat(engine.player,"RV32",160,96,160*4);}
};
bool until(const std::function<bool()>& fn,int milliseconds=5000){const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);do{if(fn())return true;std::this_thread::sleep_for(std::chrono::milliseconds(25));}while(std::chrono::steady_clock::now()<end);return fn();}
std::string json(const std::string& s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';if(c>=32)out+=static_cast<char>(c);}return out+'"';}
}
int playbackTest(const std::filesystem::path& root,const std::filesystem::path& fixture,const std::filesystem::path& report){
 std::vector<std::string> passed;std::string error,runtimeVersion;double baseline=0,enhanced=0;unsigned count=0;
 auto check=[&](bool value,const char* name){if(!value)throw std::runtime_error(name);passed.emplace_back(name);};
 try{
  check(FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),RT_MANIFEST)!=nullptr,"explicit Windows application manifest embedded");auto api=VlcApi::load(root);runtimeVersion=api->runtimeVersion;check(supportedRuntime(runtimeVersion),"official compatible libVLC loaded and all required API symbols resolved");Frames frames;
  {Engine engine(api,nullptr,false,true);frames.attach(engine);Item item{std::filesystem::absolute(fixture).wstring(),L"Synthetic original",false};engine.open(item);
   check(until([&]{return frames.count>=8&&engine.length()>10000&&engine.seekable();}),"real AVI RGB video and PCM audio decoding, duration and seeking");check(!engine.tracks(true).empty(),"actual audio tracks enumerated");const auto initial=frames.count.load();check(until([&]{return frames.count>initial+5;}),"multiple decoded frames presented through libVLC callbacks");
   engine.pause(true);check(until([&]{return api->State(engine.player)==libvlc_Paused;}),"pause acknowledged by engine");
   // Retained-frame repaint callbacks do not mean media advanced.
   auto lastChange=frames.changes.load();auto stableSince=std::chrono::steady_clock::now();check(until([&]{auto current=frames.changes.load();if(current!=lastChange){lastChange=current;stableSince=std::chrono::steady_clock::now();}return std::chrono::steady_clock::now()-stableSince>std::chrono::milliseconds(500);},4000),"paused picture reaches a stable retained frame");
   const auto pausedTime=engine.time();const auto pausedChanges=frames.changes.load();count=frames.count.load();std::this_thread::sleep_for(std::chrono::milliseconds(350));check(std::llabs(engine.time()-pausedTime)<=100&&frames.changes==pausedChanges,"paused media position and picture do not advance");engine.pause(false);check(until([&]{return frames.changes>pausedChanges+3;}),"resume produces new content frames");
   check(engine.seek(5000)&&until([&]{return engine.time()>=4800&&engine.time()<7000;}),"seek changes the actual media position");check(api->SetRate(engine.player,1.25f)==0&&std::abs(api->GetRate(engine.player)-1.25f)<.01f,"playback rate accepted and read back");api->SetRate(engine.player,1.f);
   baseline=frames.average();engine.effects.enabled=true;engine.effects.brightness=1.6f;engine.applyEffects();check(std::abs(api->GetAdjust(engine.player,libvlc_adjust_Brightness)-1.6f)<.01f,"brightness parameter accepted by libVLC");check(until([&]{enhanced=frames.average();return std::abs(enhanced-baseline)>15;}),"real decoded output changes after brightness adjustment");engine.effects.enabled=false;engine.applyEffects();check(until([&]{return std::abs(frames.average()-baseline)<4;}),"bypass restores original fixture pixels");
   check(api->EqCount()>0,"VLC equalizer presets available");engine.equalizer(0);engine.equalizer(-1);passed.emplace_back("equalizer enabled and disabled through libVLC");check(api->SetAudioDelay(engine.player,100000)==0,"audio timing adjustment accepted");engine.subtitle(fixture.parent_path()/L"captions.srt");check(until([&]{return engine.tracks(false).size()>1;}),"external SRT subtitle loaded and enumerated");check(api->SetSubtitleDelay(engine.player,150000)==0,"subtitle timing adjustment accepted");
   auto png=report.parent_path()/L"vlc-decoded-frame.png";check(engine.snapshot(png)&&until([&]{return std::filesystem::exists(png)&&std::filesystem::file_size(png)>64;}),"VLC displayed-frame PNG snapshot written");std::ifstream imageFile(png,std::ios::binary);char signature[8]{};imageFile.read(signature,8);check(std::string(signature,8)==std::string("\x89PNG\r\n\x1a\n",8),"snapshot has a valid PNG signature");
   engine.stop();check(api->State(engine.player)==libvlc_Stopped,"stop completes");const auto before=frames.count.load();engine.open(item);check(until([&]{return frames.count>before+3;}),"stop and reopen continue real decoding");
   Frames second;{Engine imported(api,nullptr,true,true);second.attach(imported);imported.open(item);check(until([&]{return second.count>=3;}),"independent imported-video decoder produces frames");check(api->GetAudioTrack(imported.player)==-1,"comparison decoder has no selected audio stream");imported.stop();}
   {NrSource sampled(api,item,160,96,0);std::optional<NrImage> image;check(until([&]{image=sampled.sample();return image.has_value();}),"native neural preview samples real decoded RGBA without invoking a model");check(image->header.width==160&&image->header.height==96&&image->original.size()==160*96*4,"preview sample geometry and bounded RGBA size");const auto pixel=(48*160+80)*4;check(image->original[pixel+3]==255&&image->original[pixel]>image->original[pixel+2],"native sampler converts VLC BGR into RGBA with opaque alpha");}
   auto rejected=report.parent_path()/L"unreviewed-model-fixture";std::filesystem::create_directories(rejected);{std::ofstream manifest(rejected/L"manifest.json");manifest<<"{}";}
   {NrProcess inspect(L"--inspect "+quote(rejected.wstring()));auto response=inspect.readText({});check(!inspect.succeeded()&&response.find("MODEL_NOT_REVIEWED")!=std::string::npos,"real worker hashes and rejects unreviewed manifest without GPU/model loading");}
   {NrProcess inspect(L"--inspect-research "+quote(rejected.wstring()));auto response=inspect.readText({});check(!inspect.succeeded()&&response.find("MODEL_NOT_REVIEWED")==std::string::npos&&!response.empty(),"research mode rejects malformed schema rather than pretending a model loaded");}
   {NrSession changed(rejected,std::string(64,'0'));check(until([&]{return changed.finished();}),"research worker terminates on changed consent fingerprint");check(!changed.ready()&&changed.status().find("MODEL_CHANGED")!=std::string::npos,"research consent must match the exact inspected manifest");}
   {NrSession denied(rejected);check(until([&]{return denied.status().find("MODEL_NOT_REVIEWED")!=std::string::npos;}),"private worker session communicates model-gate failure and remains unavailable");check(!denied.ready(),"no false ready state on native model rejection");}
   count=frames.count.load();engine.stop();
  }passed.emplace_back("players released before callback storage and runtime unload");
 }catch(const std::exception& e){error=e.what();}
 std::ofstream out(report);out<<"{\n  \"schema\":1,\n  \"runtime\":"<<json(runtimeVersion)<<",\n  \"passed\":[";for(size_t i=0;i<passed.size();++i)out<<(i?",":"")<<json(passed[i]);out<<"],\n  \"error\":"<<json(error)<<",\n  \"displayCallbacks\":"<<count<<",\n  \"originalMean\":"<<baseline<<",\n  \"adjustedMean\":"<<enhanced<<",\n  \"dlss5Inference\":false,\n  \"physicalGpuValidated\":false,\n  \"scope\":\"Synthetic local AVI, software decode, dummy audio output; real native worker model rejection. No trained-model inference, VSR, HDR, sound-device or hardware-performance certification.\"\n}\n";return error.empty()&&out.good()?0:1;
}
}
