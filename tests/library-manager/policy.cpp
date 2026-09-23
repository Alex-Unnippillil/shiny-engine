// SPDX-License-Identifier: MIT
#include "catalog.hpp"
#include <functional>
#include <iostream>
#include <random>
#include <vector>
using namespace shiny::libraries;
namespace {
int checks = 0;
void check(bool value, const char* name) { ++checks; if (!value) throw std::runtime_error(name); }
void put16(std::vector<std::uint8_t>& b, std::size_t n, std::uint16_t x) {
    b.at(n)=static_cast<std::uint8_t>(x); b.at(n+1)=static_cast<std::uint8_t>(x>>8);
}
void put32(std::vector<std::uint8_t>& b, std::size_t n, std::uint32_t x) {
    for(int i=0;i<4;++i) b.at(n+static_cast<std::size_t>(i))=static_cast<std::uint8_t>(x>>(i*8));
}
std::vector<std::uint8_t> fixture(bool pe64=true, bool dll=true) {
    std::vector<std::uint8_t> b(1024);
    put16(b,0,0x5a4d);put32(b,60,128);put32(b,128,0x4550);
    put16(b,132,pe64?0x8664:0x14c);put16(b,134,1);put16(b,148,pe64?240:224);
    put16(b,150,static_cast<std::uint16_t>(2|(dll?0x2000:0)));
    put16(b,152,pe64?0x20b:0x10b);put32(b,212,512);
    const std::size_t table=pe64?392:376;
    put32(b,table+16,512);put32(b,table+20,512);return b;
}
void rejects(const std::function<void(std::vector<std::uint8_t>&)>& corrupt,const char* name) {
    auto b=fixture();corrupt(b);bool rejected=false;
    try{(void)inspectPe(b);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,name);
}
}
int main() {
 try {
    check(slots.size()==12,"fixed scan count");
    check(candidateSlot("NVNGX_DLSS.DLL")!=nullptr,"case folding");
    check(candidateSlot("nvngx_dlss.dll.old")==nullptr,"no suffix guessing");
    check(candidateSlot("../nvngx_dlss.dll")==nullptr,"no path matching");
    check(candidateSlot("dxgi.dll")==nullptr,"proxy DLL not allowlisted");
    check(candidateSlot("nvcuda.dll")==nullptr,"driver DLL not allowlisted");
    check(candidateSlot("model.bin")==nullptr,"models are not runtimes");
    for(const auto& slot:slots)check(!blockedReason(slot.family).empty(),"all slots blocked");
    check(inspectPe(fixture()).architecture=="x64","x64 parse");
    check(inspectPe(fixture(false)).architecture=="x86","x86 parse");
    auto arm=fixture();put16(arm,132,0xaa64);check(inspectPe(arm).architecture=="arm64","arm64 parse");
    check(inspectPe(fixture()).dll,"DLL flag");check(!inspectPe(fixture(true,false)).dll,"executable flag");
    rejects([](auto& b){b.resize(10);},"short DOS header");
    rejects([](auto& b){b[0]=0;},"missing MZ");
    rejects([](auto& b){put32(b,60,0);},"overlapping DOS header");
    rejects([](auto& b){put32(b,60,0xffffffff);},"unbounded offset");
    rejects([](auto& b){b.resize(130);},"short PE header");
    rejects([](auto& b){b[128]=0;},"missing PE signature");
    rejects([](auto& b){put16(b,150,0);},"non executable image");
    rejects([](auto& b){put16(b,134,0);},"no sections");
    rejects([](auto& b){put16(b,134,97);},"section count limit");
    rejects([](auto& b){put16(b,148,65535);},"optional size bounds");
    rejects([](auto& b){put16(b,148,2);},"short optional header");
    rejects([](auto& b){put16(b,152,0);},"bad magic");
    rejects([](auto& b){put16(b,132,0x14c);},"machine/format mismatch");
    rejects([](auto& b){put16(b,132,0x9999);},"unknown architecture");
    rejects([](auto& b){put32(b,260,1000);},"directories outside optional header");
    rejects([](auto& b){b.resize(400);},"section table bounds");
    rejects([](auto& b){put32(b,212,1);},"headers overlap section table");
    rejects([](auto& b){put32(b,212,2000);},"header outside file");
    rejects([](auto& b){put32(b,412,1);},"section overlaps headers");
    rejects([](auto& b){put32(b,412,0xffffffff);},"section offset overflow");
    rejects([](auto& b){put32(b,408,0xffffffff);},"section length overflow");
    check(jsonString("\"\\\n\t")=="\"\\\"\\\\\\u000a\\u0009\"","JSON escapes");
    check(jsonString(std::string(1,'\0'))=="\"\\u0000\"","JSON NUL");
    check(jsonString("x64")=="\"x64\"","JSON normal");
    std::mt19937 rng(20260923);
    for(int i=0;i<5000;++i){auto b=fixture();for(int n=0;n<8;++n)b[static_cast<std::size_t>(rng())%b.size()]=static_cast<std::uint8_t>(rng());try{(void)inspectPe(b);}catch(const std::runtime_error&) {}}
    std::cout<<checks<<" policy checks and 5000 malformed-header mutations passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
