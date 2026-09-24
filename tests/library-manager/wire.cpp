// SPDX-License-Identifier: MIT
#include "wire.hpp"
#include <functional>
#include <iostream>
using namespace shiny::packages::wire;
int main(){
 int count=0;
 auto fails=[&](const std::function<void(FrameHeader&)>& corrupt){FrameHeader h;h.width=7;h.height=5;h.bytes=140;h.sequence=1;corrupt(h);bool rejected=false;try{validate(h);}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("malformed-frame-accepted");++count;};
 try{
  FrameHeader valid;valid.width=7;valid.height=5;valid.bytes=140;valid.sequence=1;validate(valid);
  fails([](auto& h){h.magicValue=0;});fails([](auto& h){h.abiValue=2;});fails([](auto& h){h.sequence=0;});fails([](auto& h){h.ptsMs=-1;});
  fails([](auto& h){h.width=0;});fails([](auto& h){h.height=0;});fails([](auto& h){h.width=0xffffffff;});fails([](auto& h){h.bytes=0xffffffff;});
  fails([](auto& h){h.width=960;h.height=960;h.bytes=960*960*4;});fails([](auto& h){h.command=Result;});fails([](auto& h){h.command=Reset;});
  FrameHeader reset;reset.command=Reset;reset.sequence=2;validate(reset);reset.command=Ack;validate(reset,true);
  std::cout<<count<<" rejection cases and frame/reset round-trip shapes passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
