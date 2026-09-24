// SPDX-License-Identifier: MIT
#include "preview_geometry.hpp"
#include <iostream>
#include <limits>
#include <source_location>
using namespace shiny::preview;
void check(bool b,const std::source_location loc=std::source_location::current()){if(!b)throw std::runtime_error("preview check at line "+std::to_string(loc.line()));}
int main(){try{
 auto wide=decodeSize(3840,2160);check(wide[0]==960&&wide[1]==540);
 auto square=decodeSize(4096,4096);check(square[0]==720&&square[1]==720);
 auto portrait=decodeSize(1080,1920);check(portrait[0]==540&&portrait[1]==960);
 for(unsigned w:{640u,720u,1280u,1920u,4096u})for(unsigned h:{480u,720u,1080u,4096u}){auto d=decodeSize(w,h);check(d[0]<=960&&d[1]<=960&&d[0]*d[1]<=518400);}
 check(sourceSize(515,33)==std::array<unsigned,2>{512,33});
 check(sourceSize(33,515)==std::array<unsigned,2>{33,512});
 check(sourceSize(1920,1080)==std::array<unsigned,2>{512,288});
 check(sourceSize(3840,2160,960)==wide);
 for(auto dims:{std::array<unsigned,2>{0,1080},{1920,0},{20000,1080},{16384,33}}){
  bool rejected=false;try{(void)sourceSize(dims[0],dims[1]);}catch(const std::runtime_error&){rejected=true;}check(rejected);
 }
 auto f=fit({0,0,1000,600},1920,1080);check(std::abs(f.w-1000.f)<.01f&&std::abs(f.h-562.5f)<.01f&&std::abs(f.y-18.75f)<.01f);
 auto exact=fit({0,0,100,100},200,200,true);check(exact.w==200&&exact.x==-50);
 check(fit({0,0,100,100},0,0).w==0);check(wipe(-1)==0&&wipe(4)==1&&wipe(std::numeric_limits<float>::quiet_NaN())==.5f);
 Latency timing;check(timing.p95()==0);for(int i=1;i<=100;++i)timing.add(i);check(timing.p95()==95);
 timing.add(std::numeric_limits<double>::quiet_NaN());timing.add(-1);check(timing.size()==100);
 for(int i=0;i<200;++i)timing.add(7);check(timing.size()==120&&timing.p95()==7);timing.clear();check(timing.size()==0);
 std::cout<<"Preview: decode budget, fit/1:1 geometry, wipe bounds and rolling p95 passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
