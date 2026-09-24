// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "platform.hpp"
#include <algorithm>
#include <iostream>
extern "C" std::uint32_t shinyEnhancementAbi();
extern "C" std::uint32_t shinyEnhancementRevision();
extern "C" int shinyEnhance(const std::uint8_t*,std::uint8_t*,std::uint32_t,std::uint32_t);
using namespace shiny::packages;
int main(){try{
 Bytes input;for(unsigned y=0;y<5;++y)for(unsigned x=0;x<7;++x)for(unsigned c=0;c<4;++c)input.push_back(static_cast<std::uint8_t>(c<3?(x*37+y*11+c*23)%256:(x+y)*7));
 Bytes output(input.size());
 if(shinyEnhancementAbi()!=1||shinyEnhancementRevision()!=SHINY_SPATIAL_REVISION||shinyEnhance(input.data(),output.data(),7,5))throw std::runtime_error("backend-failed");
 std::string expected=SHINY_SPATIAL_REVISION==1?"b086a1d7394bb19d3470c7ff4a8e479019e9a97e3ae1302a7b805e71fc7ec8d8":"64a9704eb22c94be4a41dc56ea42bcc85220086086d3780baca23b2d2e64a2a7";
 if(digest(output)!=expected)throw std::runtime_error("golden-frame-mismatch");
 for(std::size_t i=3;i<input.size();i+=4)if(input[i]!=output[i])throw std::runtime_error("alpha-changed");
 if(!shinyEnhance(nullptr,output.data(),7,5)||!shinyEnhance(input.data(),nullptr,7,5)||!shinyEnhance(input.data(),input.data(),7,5)||!shinyEnhance(input.data(),output.data(),960,960)||!shinyEnhance(input.data(),output.data(),0xffffffff,1))throw std::runtime_error("invalid-input-accepted");
 input.assign(maxFramePixels*4,42);output.resize(input.size());
 if(shinyEnhance(input.data(),output.data(),960,540)||output!=input)throw std::runtime_error("constant-frame-not-preserved");
 std::cout<<"Reference revision "<<SHINY_SPATIAL_REVISION<<": golden, alpha, invalid inputs and max-size flat frame passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
