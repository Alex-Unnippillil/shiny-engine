// SPDX-License-Identifier: MIT
#include "composition.hpp"
#include "approvals.hpp"
#include "sha256.h"
#include <iostream>
#include <functional>
using namespace shiny::nrwire;
int main(){int checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("NR contract check failed "+std::to_string(checks));};auto bad=[&](auto fn){bool threw=false;try{fn();}catch(...){threw=true;}check(threw);};try{
 Header h;h.width=64;h.height=48;h.bytes=64*48*4;h.sequence=1;validate(h);check(sizeof h==64);
 for(uint32_t n:{0u,32u,513u,0xffffffffu}){auto v=h;v.width=n;bad([&]{validate(v);});}
 for(float n:{-1.f,2.f,NAN,INFINITY}){auto v=h;v.tone=n;bad([&]{validate(v);});}
 for(auto i:{0,1,2,3}){auto v=h;v.padding[i]=1;bad([&]{validate(v);});}
 auto v=h;v.command=99;bad([&]{validate(v);});v=h;v.api=2;bad([&]{validate(v);});v=h;v.bytes--;bad([&]{validate(v);});
 auto response=h;response.command=Result;validateReply(response,&h);check(true);response.sequence++;bad([&]{validateReply(response,&h);});
 std::vector<uint8_t> pixels(h.bytes,100);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=static_cast<uint8_t>((i/4)%256);std::vector<float> head(h.bytes,0);
 auto out=compose(h,pixels,head,64);check(out.size()==pixels.size());for(size_t i=3;i<pixels.size();i+=4)check(out[i]==pixels[i]);
 h.blend=0;check(compose(h,pixels,head,64)==pixels);h.blend=1;std::fill(head.begin(),head.end(),.5f);out=compose(h,pixels,head,64);check(out[0]>pixels[0]);
 head[0]=NAN;bad([&]{compose(h,pixels,head,64);});head[0]=0;head[3]=INFINITY;bad([&]{compose(h,pixels,head,64);});bad([&]{compose(h,pixels,head,1);});
 check(shiny::nrpolicy::reviewed.empty());check(!shiny::nrpolicy::approved(std::string(64,'0')));
 const std::string abc="abc";check(sha256Hex(reinterpret_cast<const uint8_t*>(abc.data()),abc.size())=="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
 std::cout<<checks<<" protocol/composition/approval assertions passed. Synthetic residuals only; no trained model inference.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what();return 1;}}
