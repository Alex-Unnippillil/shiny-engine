// SPDX-License-Identifier: MIT
#include "manifest.hpp"
#include <iostream>
#include <sstream>
using namespace shiny::nrpolicy;
#include "test_fixture.hpp"
using shiny::nrpolicy::testing::fixture;
int main(){int checks=0;auto check=[&](bool x){++checks;if(!x)throw std::runtime_error("Manifest assertion "+std::to_string(checks));};auto bad=[&](auto f){bool threw=false;try{f();}catch(...){threw=true;}check(threw);};try{
 for(auto text:{"{\"x\":1,\"x\":2}","[truee]","[falsee]","[nullx]","[NaN]","[1e9999]","[01]","[.5]","[1.]","[+1]","{\"x\":\"bad\\q\"}","\"\\uD800\"","\"\\uXYZW\"","{\"x\":1,}","[1,]","{","[", "\"unterminated\\"})bad([&]{StrictJson(text).parse();});
 for(auto text:{"null","true","false","[0,-2,1.25,3e-2]","{\"x\":\"test\\n\\u0061\"}"}){StrictJson(text).parse();check(true);}
 std::string deep(25,'[');deep+="0";deep+=std::string(25,']');bad([&]{StrictJson(deep).parse();});
 for(auto path:{"../bad.bin","/bad.bin","a//b.bin","a/./b.bin","a/../b.bin","C:foo.bin","a\\foo.bin","con.bin","bad. /a.bin","x.exe","nul/x.bin"})bad([&]{safePath(path);});safePath("stages/block-01.bin");check(true);
 check(hashString(std::string(64,'A')));check(!hashString(std::string(64,'z')));check(!hashString("0"));
 auto text=fixture();auto layout=validateLayout(text);check(layout.tensors==requiredTensors().size());check(layout.bytes>100*1024*1024&&layout.bytes<256*1024*1024);check(requiredTensors().at("block70.layer0.layer")==21808);
 auto replace=[&](std::string from,std::string to){auto changed=text;auto at=changed.find(from);check(at!=std::string::npos);changed.replace(at,from.size(),to);bad([&]{validateLayout(changed);});};
 replace("\"blockCount\":71","\"blockCount\":70");replace("\"stageOffset\":0","\"stageOffset\":-1");replace("\"stageOffset\":0","\"stageOffset\":0.5");replace("\"stageOffset\":0","\"stageOffset\":true");replace("\"stageOffset\":0","\"stageOffset\":268435456");replace("\"byteLength\":21696","\"byteLength\":1");replace("\"stage\":\"all\"","\"stage\":\"missing\"");replace("\"name\":\"block0.layer0.layer\"","\"name\":\"nope\"");replace("stage.bin","../stage.bin");
 bad([&]{validateLayout("{}");});bad([&]{integer(StrictJson("1.1").parse(),100);});
 std::cout<<checks<<" strict parser, tensor and path assertions passed. Metadata fixture only; no trained model.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what();return 1;}}
