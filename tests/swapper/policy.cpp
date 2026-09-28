// SPDX-License-Identifier: MIT
#include "policy.hpp"
#include <iostream>
#include <vector>
using namespace shiny::swapper;
int count=0;
void check(bool b){++count;if(!b)throw std::runtime_error("policy assertion failed");}
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const std::runtime_error&){bad=true;}check(bad);}
int main(){try{
    check(recognize(L"DLSS 5 Swapper.exe")==Tool::dlss5);
    check(recognize(L"DLSS5-Swapper-2.2.7-portable.exe")==Tool::dlss5);
    check(recognize(L"DLSS5-Swapper.exe")==Tool::dlss5);
    check(recognize(L"DLSS Swapper.exe")==Tool::classic);
    check(recognize(L"dlss-swapper.EXE")==Tool::classic);
    for(auto n:{L"DLSS5-Swapper-Setup-2.2.7.exe",L"cmd.exe",L"vlc.exe",L"nvngx_dlss.dll",L"DLSS5-Swapper-2..7-portable.exe",L"DLSS5-Swapper-2.7-portable.exe",L"DLSS5-Swapper-..-portable.exe",L"DLSS5-Swapper-2.2.7-portable.exe.bat",L"C:/DLSS5-Swapper.exe",L"dlss5-swapper-1111111.2.3-portable.exe"}) rejects([&]{(void)recognize(n);});
    check(localSyntax(L"C:\\Tools\\DLSS 5 Swapper.exe"));
    check(localSyntax(L"c:/My tools/été/DLSS5-Swapper.exe"));
    for(auto p:{L"DLSS5-Swapper.exe",L"\\\\server\\share\\DLSS5-Swapper.exe",L"\\\\?\\C:\\Tools\\DLSS5-Swapper.exe",L"C:\\Tools\\..\\DLSS5-Swapper.exe",L"C:\\Tools \\DLSS5-Swapper.exe",L"C:/Tools./DLSS5-Swapper.exe",L"C:/Tools//DLSS5-Swapper.exe",L"C:/Tools/DLSS5-Swapper.exe:stream",L"C:/Tools/DLSS5-Swapper.exe/",L"C:/Tools/./DLSS5-Swapper.exe"})check(!localSyntax(p));
    std::vector<std::uint8_t> b(1024);
    auto put16=[&](std::size_t at,unsigned v){b[at]=static_cast<std::uint8_t>(v);b[at+1]=static_cast<std::uint8_t>(v>>8);};
    put16(0,0x5a4d);put16(60,128);put16(128,0x4550);put16(132,0x8664);put16(134,1);put16(148,240);put16(150,2);put16(152,0x20b);
    executableHeader(b,b.size());check(true);
    rejects([&]{executableHeader(b,MaxBytes+1);});
    put16(150,0x2002);rejects([&]{executableHeader(b,b.size());});put16(150,2);
    put16(132,0xaa64);rejects([&]{executableHeader(b,b.size());});put16(132,0x8664);
    put16(134,0);rejects([&]{executableHeader(b,b.size());});put16(134,1);
    put16(148,0);rejects([&]{executableHeader(b,b.size());});put16(148,240);
    put16(60,65535);rejects([&]{executableHeader(b,b.size());});
    for(std::size_t n=0;n<64;++n)rejects([&]{executableHeader(std::span(b.data(),n),b.size());});
    std::cout<<count<<" optional swapper policy assertions passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
