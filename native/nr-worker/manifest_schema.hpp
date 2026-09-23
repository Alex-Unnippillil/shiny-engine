// SPDX-License-Identifier: MIT
// Data-only intake for the pinned 310.8.0 / 71-block OpenDLSS-NR graph.
// Layout bounds derived from upstream nr_graph.cpp at 9d08f418 (MIT).
#pragma once
#include "strict_json.hpp"
#include "research.hpp"
#include <algorithm>
#include <set>
namespace shiny::nrpolicy {
inline constexpr uint64_t maxModelBytes=256ull*1024*1024;
struct StageSpec{std::string id,file,digest;uint64_t bytes=0;};
struct TensorSpec{std::string name,stage;uint64_t offset=0,bytes=0;unsigned block=0,layer=0;};
struct ManifestSpec{std::vector<StageSpec> stages;std::vector<TensorSpec> tensors;uint64_t bytes=0;};
inline bool identifier(std::string_view s){return !s.empty()&&s.size()<=160&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.+")==std::string_view::npos;}
inline bool stagePath(std::string_view s){
 if(s.empty()||s.size()>240||s.front()=='/'||s.back()=='/'||!s.ends_with(".bin"))return false;
 if(s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-./")!=std::string_view::npos)return false;
 size_t start=0;
 while(start<s.size()){
  auto end=s.find('/',start);if(end==std::string_view::npos)end=s.size();auto part=s.substr(start,end-start);
  if(part.empty()||part=="."||part==".."||part.back()=='.')return false;
  // Disallow Win32 reserved device basenames, including extensions.
  std::string base(part.substr(0,part.find('.')));for(char& c:base)if(c>='a'&&c<='z')c-=32;
  if(base=="CON"||base=="PRN"||base=="AUX"||base=="NUL"||(base.size()==4&&(base.starts_with("COM")||base.starts_with("LPT"))&&base[3]>='1'&&base[3]<='9'))return false;
  start=end+1;
 }return true;
}
inline std::string lowerAscii(std::string text){for(char& c:text)if(c>='A'&&c<='Z')c+=32;return text;}
struct Requirement{uint64_t minimum;bool exact=false;};
inline uint64_t fusedEnd(unsigned c,bool upsample=false){
 const uint64_t heads=c/32,experts=c>=64?c/32:0;
 const uint64_t ffn=experts?experts*(c*128ull+128ull*32+32ull*c):c*128ull+128ull*c;
 uint64_t qkv;
 if(upsample){auto narrow=c==32?16u:0u;auto ffnScale=ffn+2ull*c*c+narrow;auto transition=ffnScale+2ull*c+narrow;qkv=transition+2ull*c;}
 else qkv=ffn+16+2ull*c+16;
 auto scale=qkv+3ull*c*c+heads*8192;
 auto projection=scale+((heads*4+15)/16)*16;
 return projection+uint64_t(c)*c+2ull*c;
}
inline std::map<std::string,Requirement> requiredTensors(){
 std::map<std::string,Requirement> out;
 auto add=[&](unsigned b,unsigned l,uint64_t n,bool exact=false){out.emplace("block"+std::to_string(b)+".layer"+std::to_string(l)+".layer",Requirement{n,exact});};
 add(0,0,21696,true);
 for(unsigned b=1;b<=22;++b){unsigned c=b<=4?32:b<=8?64:b<=14?128:256;bool transition=b==4||b==8||b==14||b==22;add(b,0,fusedEnd(c)+(transition?2ull*c*c:0));}
 for(unsigned b=23;b<=47;++b){
  if(b>=31&&b<=38){add(b,0,1024ull*4096);add(b,1,4096ull*1024+2048);add(b,2,128+1024ull*3072);add(b,4,1024ull*1024+2048);}
  else if(b==39)add(b,0,1024ull*512+1024);
  else{add(b,0,524288);add(b,1,263168);add(b,2,917568);add(b,3,263168);if(b==30)add(b,4,524288);}
 }
 for(unsigned b=48;b<=69;++b){unsigned c=b<=55?256:b<=61?128:b<=65?64:32;bool up=b==48||b==56||b==62||b==66;add(b,0,fusedEnd(c,up)+(up?16:0),up);}
 add(70,0,21808,true);return out;
}
inline ManifestSpec validateManifest(const strict::Value& value){
 if(value.at("totals").at("blockCount").integer(71)!=71)throw std::runtime_error("Only the 71-block native graph is supported.");
 const auto& stages=value.at("stages");const auto& tensors=value.at("tensors");
 if(stages.kind!=strict::Value::Array||stages.array.empty()||stages.array.size()>256||tensors.kind!=strict::Value::Array||tensors.array.empty()||tensors.array.size()>1024)
  throw std::runtime_error("Model requires 1–256 stages and 1–1024 tensors.");
 ManifestSpec result;std::map<std::string,uint64_t> sizes;std::set<std::string> paths;
 for(const auto& entry:stages.array){
  StageSpec s{entry.at("id").text(),entry.at("file").text(),lowerAscii(entry.at("sha256").text()),entry.at("packedByteLength").integer(maxModelBytes)};
  if(!identifier(s.id)||!stagePath(s.file)||!hashText(s.digest)||!s.bytes||!sizes.emplace(s.id,s.bytes).second||!paths.insert(lowerAscii(s.file)).second)
   throw std::runtime_error("Invalid or duplicate model stage identity, path, size or SHA-256.");
  result.bytes+=s.bytes;if(result.bytes>maxModelBytes)throw std::runtime_error("Model exceeds the 256 MiB stage budget.");result.stages.push_back(s);
 }
 auto required=requiredTensors();std::set<std::string> names;uint64_t allocated=0;
 for(const auto& entry:tensors.array){
  TensorSpec t{entry.at("name").text(),entry.at("stage").text(),entry.at("stageOffset").integer(maxModelBytes),entry.at("byteLength").integer(maxModelBytes),static_cast<unsigned>(entry.at("block").integer(70)),static_cast<unsigned>(entry.at("layer").integer(32))};
  auto parameter=entry.at("parameter").text();
  auto expected="block"+std::to_string(t.block)+".layer"+std::to_string(t.layer)+"."+parameter;
  if(!identifier(parameter)||t.name!=expected||!names.insert(t.name).second||!sizes.contains(t.stage)||!t.bytes||t.offset>sizes.at(t.stage)||t.bytes>sizes.at(t.stage)-t.offset)
   throw std::runtime_error("Invalid tensor name, stage or byte range.");
  allocated+=t.bytes;if(allocated>maxModelBytes)throw std::runtime_error("Tensor allocations exceed the 256 MiB budget.");
  if(auto it=required.find(t.name);it!=required.end()){
   if(t.bytes<it->second.minimum||(it->second.exact&&t.bytes!=it->second.minimum))throw std::runtime_error("Tensor layout mismatch: "+t.name);
   required.erase(it);
  }
  result.tensors.push_back(t);
 }
 if(!required.empty())throw std::runtime_error("Missing required graph tensor: "+required.begin()->first);
 return result;
}
inline ManifestSpec parseManifest(std::string_view text){return validateManifest(strict::parse(text));}
}
