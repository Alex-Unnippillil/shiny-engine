// SPDX-License-Identifier: MIT
#pragma once
// Validate untrusted local data before the pinned upstream parser/loader sees it.
#include "json.h"
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string_view>
#include <algorithm>
namespace shiny::nrpolicy {
class StrictJson {
 const std::string& text; size_t pos=0,nodes=0;
 [[noreturn]] void fail() const {throw std::runtime_error("Invalid model JSON near byte "+std::to_string(pos));}
 void space(){while(pos<text.size()&&(text[pos]==' '||text[pos]=='\t'||text[pos]=='\r'||text[pos]=='\n'))++pos;}
 char peek(){space();return pos<text.size()?text[pos]:'\0';}
 void expect(char c){if(peek()!=c)fail();++pos;}
 unsigned hex(){unsigned n=0;for(int i=0;i<4;++i){if(pos>=text.size())fail();char c=text[pos++];n<<=4;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else fail();}return n;}
 std::string string(){expect('"');std::string out;
  while(pos<text.size()){unsigned char c=text[pos++];if(c=='"')return out;if(c<32)fail();
   if(c=='\\'){if(pos>=text.size())fail();c=text[pos++];switch(c){case '"':case '\\':case '/':out+=c;break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
    case 'u':{unsigned u=hex();if(u>=0xd800&&u<=0xdfff)fail(); // Identifiers in this format are ASCII; reject surrogate ambiguity.
     if(u<128)out+=static_cast<char>(u);else if(u<2048){out+=static_cast<char>(0xc0|(u>>6));out+=static_cast<char>(0x80|(u&63));}else{out+=static_cast<char>(0xe0|(u>>12));out+=static_cast<char>(0x80|((u>>6)&63));out+=static_cast<char>(0x80|(u&63));}break;}
    default:fail();}}
   else out+=c;if(out.size()>16384)fail();
  }fail();
 }
 json::Value value(unsigned depth){if(depth>24||++nodes>100000)fail();json::Value v;char c=peek();
  if(c=='{'){++pos;v.kind=json::Value::Object;if(peek()=='}'){++pos;return v;}for(;;){auto key=string();expect(':');auto x=value(depth+1);if(!v.object.emplace(key,std::move(x)).second)fail();c=peek();++pos;if(c=='}')break;if(c!=',')fail();}}
  else if(c=='['){++pos;v.kind=json::Value::Array;if(peek()==']'){++pos;return v;}for(;;){v.array.push_back(value(depth+1));c=peek();++pos;if(c==']')break;if(c!=',')fail();}}
  else if(c=='"'){v.kind=json::Value::String;v.string=string();}
  else if(c=='t'||c=='f'||c=='n'){std::string token=c=='t'?"true":c=='f'?"false":"null";if(text.compare(pos,token.size(),token))fail();pos+=token.size();v.kind=c=='n'?json::Value::Null:json::Value::Bool;v.boolean=c=='t';}
  else{size_t start=pos;if(c=='-')++pos;if(pos>=text.size())fail();if(text[pos]=='0')++pos;else{if(text[pos]<'1'||text[pos]>'9')fail();while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;}
   auto digits=[&]{size_t begin=pos;while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;if(begin==pos)fail();};
   if(pos<text.size()&&text[pos]=='.'){++pos;digits();}if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;digits();}
   if(pos-start>64)fail();v.kind=json::Value::Number;v.number=std::stod(text.substr(start,pos-start));if(!std::isfinite(v.number))fail();
  }return v;
 }
 public:explicit StrictJson(const std::string& input):text(input){}
 json::Value parse(){if(text.empty()||text.size()>2*1024*1024)fail();auto v=value(0);space();if(pos!=text.size())fail();return v;}
};
inline uint64_t integer(const json::Value& v,uint64_t maximum){if(v.kind!=json::Value::Number||!std::isfinite(v.number)||v.number<0||v.number>static_cast<double>(maximum)||std::floor(v.number)!=v.number)throw std::runtime_error("Model integer is invalid or exceeds its bound.");return static_cast<uint64_t>(v.number);}
inline std::string field(const json::Value& v,size_t maximum=240){if(v.kind!=json::Value::String||v.string.empty()||v.string.size()>maximum)throw std::runtime_error("Invalid model string.");return v.string;}
inline bool hashString(std::string_view s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});}
inline std::string lowerHash(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
inline void safePath(const std::string& name){
 if(name.empty()||name.size()>240||!name.ends_with(".bin"))throw std::runtime_error("Stage path must be a relative .bin path.");
 size_t start=0;for(size_t i=0;i<=name.size();++i){if(i<name.size()&&name[i]!='/'){char c=name[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))throw std::runtime_error("Unsafe stage path.");continue;}
  auto part=name.substr(start,i-start);if(part.empty()||part=="."||part==".."||part.back()=='.')throw std::runtime_error("Unsafe stage path component.");auto stem=lowerHash(part.substr(0,part.find('.')));
  if(stem=="con"||stem=="prn"||stem=="aux"||stem=="nul"||stem.starts_with("com")||stem.starts_with("lpt"))throw std::runtime_error("Reserved stage path.");start=i+1;
 }
}
// Required raw tensor byte ranges, from the pinned graph's fused/split/ViT layouts.
// This is a bounds check, not a learned-model authenticity/quality test.
inline std::map<std::string,uint64_t> requiredTensors(){
 std::map<std::string,uint64_t> out;auto add=[&](int b,int l,uint64_t n){out["block"+std::to_string(b)+".layer"+std::to_string(l)+".layer"]=n;};
 auto fused=[](uint64_t c,bool up){uint64_t f=c==32?8192:(c/32)*(c*128+128*32+32*c);uint64_t n=f+(up?2*c*c:16)+2*c+(c==32||!up?16:0);if(up)n+=2*c+(c==32?16:0);return n+3*c*c+(c/32)*8192+(((c/32)*4+15)/16)*16+c*c+2*c;};
 add(0,0,21696);for(int b=1;b<=22;++b){uint64_t c=b<5?32:b<9?64:b<15?128:256;add(b,0,fused(c,false)+((b==4||b==8||b==14||b==22)?2*c*c:0));}
 for(int b=23;b<=47;++b){if(b==39){add(b,0,1024*512+1024);continue;}if(b>=31&&b<=38){add(b,0,1024*4096);add(b,1,4096*1024+2048);add(b,2,128+1024*3072);add(b,4,1024*1024+2048);}else{add(b,0,524288);add(b,1,262144+1024);add(b,2,786432+131072+64);add(b,3,262144+1024);if(b==30)add(b,4,512*1024);}}
 for(int b=48;b<=69;++b){uint64_t c=b<56?256:b<62?128:b<66?64:32;bool up=b==48||b==56||b==62||b==66;add(b,0,fused(c,up)+(up?16:0));}add(70,0,21808);return out;
}
struct StageInfo{std::string id,file,hash;uint64_t bytes;};
struct Layout {std::vector<StageInfo> stages;size_t tensors=0;uint64_t bytes=0;};
inline Layout validateLayout(const std::string& text){auto m=StrictJson(text).parse();if(m.kind!=json::Value::Object||integer(m["totals"]["blockCount"],71)!=71)throw std::runtime_error("Expected the 71-block OpenDLSS-NR layout.");
 const auto& stages=m["stages"];const auto& tensors=m["tensors"];if(stages.kind!=json::Value::Array||stages.array.empty()||stages.size()>256||tensors.kind!=json::Value::Array||tensors.size()>4096)throw std::runtime_error("Invalid model stage/tensor counts.");
 Layout result;std::map<std::string,uint64_t> sizes;std::set<std::string> paths;constexpr uint64_t limit=256ull*1024*1024;
 for(const auto& s:stages.array){StageInfo info{field(s["id"],120),field(s["file"]),field(s["sha256"],64),integer(s["packedByteLength"],limit)};safePath(info.file);if(!info.bytes||!hashString(info.hash)||!sizes.emplace(info.id,info.bytes).second||!paths.insert(lowerHash(info.file)).second)throw std::runtime_error("Duplicate or invalid model stage.");result.bytes+=info.bytes;if(result.bytes>limit)throw std::runtime_error("Model exceeds the 256 MiB stage budget.");result.stages.push_back(std::move(info));}
 auto required=requiredTensors();std::set<std::string> names;uint64_t raw=0;
 for(const auto& t:tensors.array){auto name=field(t["name"]),stage=field(t["stage"],120),parameter=field(t["parameter"],48);auto b=integer(t["block"],70),l=integer(t["layer"],32),offset=integer(t["stageOffset"],limit),bytes=integer(t["byteLength"],limit);
  if(name!="block"+std::to_string(b)+".layer"+std::to_string(l)+"."+parameter||!names.insert(name).second||!sizes.count(stage)||!bytes||offset>sizes.at(stage)||bytes>sizes.at(stage)-offset)throw std::runtime_error("Tensor identity, range or stage is invalid.");
  raw+=bytes;if(raw>limit)throw std::runtime_error("Tensor allocation budget exceeded.");auto it=required.find(name);if(it!=required.end()){if(bytes<it->second)throw std::runtime_error("Undersized graph tensor: "+name);if((b==0||b==48||b==56||b==62||b==66||b==70)&&l==0&&bytes!=it->second)throw std::runtime_error("Incompatible fused tensor length: "+name);required.erase(it);}
 }
 if(!required.empty())throw std::runtime_error("Missing graph tensor: "+required.begin()->first);result.tensors=tensors.size();return result;
}
}
