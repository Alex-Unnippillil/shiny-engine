// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "platform.hpp"
#include <algorithm>
#include <iostream>
#include <random>
extern "C" int shinyEnhance(const std::uint8_t*,std::uint8_t*,std::uint32_t,std::uint32_t);
using namespace shiny::packages;
void require(bool condition,const char* label){if(!condition)throw std::runtime_error(label);}
Bytes process(const Bytes& input,unsigned w,unsigned h){Bytes output(input.size());require(shinyEnhance(input.data(),output.data(),w,h)==0,"process");return output;}
int main(){try{
 Bytes input;for(unsigned y=0;y<9;++y)for(unsigned x=0;x<11;++x)for(unsigned c=0;c<4;++c)input.push_back(static_cast<std::uint8_t>(c<3?(x*x*13+y*17+c*23)%256:255));
 auto output=process(input,11,9);
 require(output!=input,"opaque detail fixture unchanged");
 require(digest(output)=="7f461021144af40c80f9dbefb1c058ee693bcaeaff9fa8146e3eae6072f84916","independent opaque golden");
 // Uniform, low-amplitude noise, hard step and transparency must not acquire halos.
 for(int mode=0;mode<4;++mode){Bytes pixels(17*13*4,255);
  for(unsigned y=0;y<13;++y)for(unsigned x=0;x<17;++x){auto i=(y*17+x)*4;int value=mode==0?96:mode==1?96+int((x+y)%3):mode==2?(x<8?40:210):int((x*39+y*71)%256);
   for(unsigned c=0;c<3;++c){pixels[i+c]=static_cast<std::uint8_t>(value);}
   pixels[i+3]=mode==3?127:255;}
  require(process(pixels,17,13)==pixels,"noise/plateau/step/alpha protection");
 }
 std::mt19937 random(903);unsigned changed=0;
 for(unsigned iteration=0;iteration<100;++iteration){Bytes pixels(31*19*4);for(auto& c:pixels)c=static_cast<std::uint8_t>(random());for(std::size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
  auto filtered=process(pixels,31,19);require(filtered==process(pixels,31,19),"determinism");
  for(std::size_t i=0;i<pixels.size();i+=4){int delta=int(filtered[i])-pixels[i];require(std::abs(delta)<=12,"detail bound");changed+=delta!=0;
   require(int(filtered[i+1])-pixels[i+1]==delta&&int(filtered[i+2])-pixels[i+2]==delta,"chroma differences");require(filtered[i+3]==pixels[i+3],"alpha");}
 }
 require(changed>100,"random detail never processed");
 std::cout<<"Adaptive detail: opaque golden, noise/step/alpha protection, 100 deterministic randomized frames and chroma/detail bounds passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
