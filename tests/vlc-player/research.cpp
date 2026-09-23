// SPDX-License-Identifier: MIT
#include "manifest_schema.hpp"
#include <iostream>
#include <random>
using namespace shiny::nrpolicy;
static unsigned count=0;
void check(bool value){++count;if(!value)throw std::runtime_error("assertion "+std::to_string(count));}
template<class F> void bad(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed);}
std::string fixture(){
 auto required=requiredTensors();std::string tensors;uint64_t total=0;
 for(const auto& [name,r]:required){auto first=name.find('.');auto second=name.find('.',first+1);auto block=name.substr(5,first-5);auto layer=name.substr(first+6,second-first-6);
  if(!tensors.empty())tensors+=',';
  tensors+="{\"name\":\""+name+"\",\"block\":"+block+",\"layer\":"+layer+",\"parameter\":\"layer\",\"stage\":\"s\",\"stageOffset\":"+std::to_string(total)+",\"byteLength\":"+std::to_string(r.minimum)+"}";total+=r.minimum;
 }
 return "{\"totals\":{\"blockCount\":71},\"stages\":[{\"id\":\"s\",\"file\":\"stage.bin\",\"packedByteLength\":"+std::to_string(total)+",\"sha256\":\""+std::string(64,'0')+"\"}],\"tensors\":["+tensors+"]}";
}
int main(){try{
 check(reviewed.empty());bad([]{authorize(Access::Reviewed,std::string(64,'a'));});bad([]{authorize(Access::LocalResearch,std::string(64,'a'),std::string(64,'b'));});
 check(!authorize(Access::LocalResearch,std::string(64,'a'),std::string(64,'a')));check(reviewed.empty());bad([]{authorize(Access::LocalResearch,"0","0");});
 auto data=fixture();auto parsed=strict::parse(data);auto model=validateManifest(parsed);check(model.stages.size()==1);check(model.tensors.size()==requiredTensors().size());check(model.bytes>100*1024*1024&&model.bytes<maxModelBytes);
 for(auto text:{"", "{} trailing", "{\"a\":1,\"a\":2}","[1,]","{\"a\":truex}","[01]","[1e999]","[-]","[.5]","[1.]","[+1]","[nan]","{\"a\": \"\\x\"}","[\"\\ud800\"]","[\"\\u000\"]","{\"a\":1,\"\\u0061\":2}"})bad([&]{strict::parse(text);});
 check(strict::parse("{\"t\":true,\"f\":false,\"n\":null,\"u\":\"\\ud83d\\ude00\"}").at("u").text().size()==4);
 bad([]{strict::parse(std::string(20,'[')+"0"+std::string(20,']'));});bad([]{strict::parse("\""+std::string(5000,'a')+"\"");});bad([]{strict::parse(std::string(2*1024*1024+1,' '));});
 bad([]{strict::parse(std::string("\"")+char(0xc0)+char(0xaf)+"\"");});
 for(auto p:{"../evil.bin","/absolute.bin","C:/evil.bin","sub\\evil.bin","foo//a.bin","./a.bin","a/../b.bin","a/CON.bin","Lpt1.bin","evil.BIN","a .bin","x. /a.bin","a./b.bin"})check(!stagePath(p));
 check(stagePath("stages/layer_0.bin"));check(stagePath("normal.bin"));
 auto mutate=[&](auto fn){auto copy=parsed;fn(copy);bad([&]{validateManifest(copy);});};
 mutate([](auto& v){v.object["totals"].object["blockCount"].number=72;});
 mutate([](auto& v){v.object["stages"].array.push_back(v.object["stages"].array[0]);});
 mutate([](auto& v){v.object["tensors"].array.push_back(v.object["tensors"].array[0]);});
 mutate([](auto& v){v.object["tensors"].array.erase(v.object["tensors"].array.begin());});
 mutate([](auto& v){v.object["tensors"].array[0].object["stageOffset"].number=-1;});
 mutate([](auto& v){v.object["tensors"].array[0].object["stageOffset"].number=0.5;});
 mutate([](auto& v){v.object["tensors"].array[0].object["byteLength"].number=4;});
 mutate([](auto& v){v.object["tensors"].array[0].object["name"].string="block999.layer0.layer";});
 mutate([](auto& v){v.object["tensors"].array[0].object["stage"].string="unknown";});
 mutate([](auto& v){v.object["stages"].array[0].object["sha256"].string="garbage";});
 mutate([](auto& v){v.object["stages"].array[0].object["packedByteLength"].kind=strict::Value::String;});
 for(const auto& [name,r]:requiredTensors()){check(r.minimum>0&&r.minimum<8*1024*1024);}
 // Every truncation of a valid manifest must be rejected; this is parser coverage, not model inference.
 for(size_t i=0;i<data.size();i+=17)bad([&]{strict::parse(std::string_view(data).substr(0,i));});
 std::mt19937 rng(7);for(int i=0;i<2000;++i){std::string fuzz;for(unsigned n=rng()%128;n;--n)fuzz+=char(rng()%256);try{strict::parse(fuzz);}catch(const std::exception&){}++count;}
 std::cout<<count<<" research authorization / strict manifest assertions passed. No model inference.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
