// SPDX-License-Identifier: MIT
#include "store.hpp"
#include "catalog.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
using namespace shiny::packages;
namespace {
int count=0;
void require(bool yes,const char* message){++count;if(!yes)throw std::runtime_error(message);}
void rejects(const std::function<void()>& test,const char* message){bool rejected=false;try{test();}catch(const std::exception&){rejected=true;}require(rejected,message);}
Bytes image(int revision){Bytes b(1024);auto p16=[&](int offset,std::uint16_t v){b[static_cast<std::size_t>(offset)]=static_cast<std::uint8_t>(v);b[static_cast<std::size_t>(offset+1)]=static_cast<std::uint8_t>(v>>8);};auto p32=[&](int offset,std::uint32_t v){p16(offset,static_cast<std::uint16_t>(v));p16(offset+2,static_cast<std::uint16_t>(v>>16));};p16(0,0x5a4d);p32(60,128);p32(128,0x4550);p16(132,0x8664);p16(134,1);p16(148,240);p16(150,0x2002);p16(152,0x20b);p32(212,512);p32(408,512);p32(412,512);b[600]=static_cast<std::uint8_t>(revision);return b;}
struct Fixture {
 fs::path base,one,two,root;std::string first,second,raw;Policy trust;
 Fixture(){
  auto serial=std::chrono::steady_clock::now().time_since_epoch().count();base=fs::temp_directory_path()/textPath("shiny-store-"+std::to_string(serial));makeDirectory(base);one=base/"one";two=base/"two";root=base/"store";
  for(int revision:{1,2}){
   auto folder=revision==1?one:two;makeDirectory(folder);auto bytes=image(revision);
   auto manifest="{\"schemaVersion\":1,\"id\":\"shiny-spatial\",\"version\":\"1."+std::to_string(revision)+".0\",\"family\":\"shiny-spatial-reference\",\"architecture\":\"x64\",\"abi\":1,\"license\":\"MIT\",\"source\":\"test-only-synthetic\",\"dependencySet\":\"reference-v1\",\"files\":[{\"name\":\"shiny_spatial.dll\",\"sha256\":\""+digest(bytes)+"\",\"size\":1024}]}";
   writeNew(folder/"manifest.json",manifest);writeNew(folder/"shiny_spatial.dll",bytes);
   (revision==1?first:second)=digest(manifest);if(revision==1)raw=manifest;
  }trust.approvedDigests={first,second};
 }
 ~Fixture(){std::error_code e;fs::remove_all(base,e);}
};
void noProbe(const fs::path&,const std::string&,std::stop_token){}
struct SimulatedCrash{};
}
int main(){try{
 require(digest("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 vector");
 {
  Fixture f;auto m=parseManifest(f.raw);require(f.trust.blockReason(m).empty(),"synthetic policy accepted only in test");require(!Policy{}.approved(m),"production empty catalog denies");
  auto mutate=[&](std::string from,std::string to){auto raw=f.raw;auto n=raw.find(from);require(n!=raw.npos,"mutation found");raw.replace(n,from.size(),to);rejects([&]{parseManifest(raw);},"schema mutation rejected");};
  mutate("\"schemaVersion\":1","\"schemaVersion\":1,\"schemaVersion\":1");
  mutate("\"abi\":1","\"abi\":true");mutate("\"abi\":1","\"abi\":17");mutate("\"size\":1024","\"size\":-1");
  mutate("shiny_spatial.dll","../shiny_spatial.dll");mutate("shiny_spatial.dll","dxgi.dll");mutate("\"files\":[","\"files\":{");mutate("\"source\"","\"unknown\"");
  mutate("\"id\":\"shiny-spatial\"","\"id\":null");mutate("\"version\":\"1.1.0\"","\"version\":\"bad\\u0000value\"");
  rejects([&]{parseManifest(f.raw+"garbage");},"trailing JSON rejected");rejects([&]{parseManifest("{}");},"empty manifest rejected");
 }
 {
  Fixture f;Store s(f.root,f.trust);require(s.importFolder(f.one)==f.first,"import identity");require(s.importFolder(f.one)==f.first,"idempotent import");
  require(s.list().find("quarantined")!=std::string::npos,"import remains quarantine");require(s.selected().empty(),"import does not activate");
  rejects([&]{s.activate(f.first,noProbe);},"must stage first");s.stage(f.first);s.activate(f.first,noProbe);require(s.selected()==f.first,"selection commit");
  s.importFolder(f.two);s.stage(f.second);rejects([&]{s.activate(f.second,[](auto&,auto&,auto){throw std::runtime_error("probe-failed");});},"failed probe rejected");require(s.selected()==f.first,"failed probe preserves first");
  s.activate(f.second,noProbe);require(s.selected()==f.second,"second selection");s.rollback(noProbe);require(s.selected()==f.first,"rollback restores first");s.rollback(noProbe);require(s.selected()==f.first,"rollback idempotent");
  rejects([&]{s.remove(f.first);},"selected package protected");
  {VerifiedLease lease(f.root,f.second,f.trust);rejects([&]{s.remove(f.second);},"leased package protected");}
  s.remove(f.second);require(!fs::exists(f.root/"packages"/f.second),"unselected package removed");
  s.original();require(s.selected().empty(),"original restored");s.original();s.rollback(noProbe);require(s.selected()==f.first,"original keeps rollback target");
  require(s.list().find(pathText(f.root))==std::string::npos,"listing redacted");require(s.history().find(pathText(f.root))==std::string::npos,"history redacted");
 }
 {
  Fixture f;{Store s(f.root,{});s.importFolder(f.one);require(s.verify(f.first)=="not-in-build-pinned-catalog","unapproved inspection remains blocked");rejects([&]{s.stage(f.first);},"untrusted stage denied");}
  Store s(f.root,f.trust);require(s.selected().empty(),"restart does not activate");s.stage(f.first);
  auto library=f.root/"packages"/f.first/"shiny_spatial.dll";{std::fstream file(library,std::ios::in|std::ios::out|std::ios::binary);file.seekp(600);file.put(9);}
  rejects([&]{s.verify(f.first);},"tamper reverified");rejects([&]{s.activate(f.first,noProbe);},"tampered activation denied");
 }
 {
  Fixture f;Store s(f.root,f.trust);std::stop_source stop;stop.request_stop();rejects([&]{s.importFolder(f.one,stop.get_token());},"cancelled import");require(s.list().find(f.first)==std::string::npos,"cancelled import unregistered");
  writeNew(f.one/"extra.txt","unexpected");rejects([&]{s.importFolder(f.one);},"extra content rejected");
 }
 for(auto point:{"import-journal","import-directory","import-manifest","import-component","import-promoted","import-committed"}){
  Fixture f;try{Store s(f.root,f.trust,[&](auto p){if(p==point)throw SimulatedCrash{};});s.importFolder(f.one);}catch(const SimulatedCrash&){}
  Store recovered(f.root,f.trust);require(recovered.selected().empty(),"recovery never activates imports");require(recovered.importFolder(f.one)==f.first,"import recovery permits retry");require(!fs::exists(f.root/"staging"/f.first),"staging recovered");
 }
 for(auto point:{"selection-pending","selection-probed","selection-committed"}){
  Fixture f;{Store s(f.root,f.trust);s.importFolder(f.one);s.stage(f.first);s.activate(f.first,noProbe);s.importFolder(f.two);s.stage(f.second);}
  try{Store s(f.root,f.trust,[&](auto p){if(p==point)throw SimulatedCrash{};});s.activate(f.second,noProbe);}catch(const SimulatedCrash&){}
  Store recovered(f.root,f.trust);require(recovered.selected()==(std::string_view(point)=="selection-committed"?f.second:f.first),"selection journal atomicity");
 }
 for(auto point:{"delete-pending","delete-files"}){
  Fixture f;{Store s(f.root,f.trust);s.importFolder(f.one);}
  try{Store s(f.root,f.trust,[&](auto p){if(p==point)throw SimulatedCrash{};});s.remove(f.first);}catch(const SimulatedCrash&){}
  Store recovered(f.root,f.trust);require(!fs::exists(f.root/"packages"/f.first),"delete recovered");require(recovered.list().find(f.first)==std::string::npos,"delete catalog recovered");
 }
 {
  Fixture f;makeDirectory(f.root);writeNew(f.root/"unrelated.txt","do not touch");rejects([&]{Store s(f.root,f.trust);},"nonempty root refused");require(fs::exists(f.root/"unrelated.txt"),"unrelated file preserved");
 }
 std::cout<<count<<" store/schema/recovery assertions passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<"assertion "<<count<<": "<<e.what()<<'\n';return 1;}}
