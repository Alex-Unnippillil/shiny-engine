// SPDX-License-Identifier: MIT
#pragma once
#include "protocol.hpp"
#include "numeric.h"
#include <bit>
#include <vector>
namespace shiny::nrwire {
// Implements the pinned upstream independent-frame composition contract.
// The fourth model channel is not treated as fabricated motion/history.
inline float halfTruncate(float value){
 uint32_t b=std::bit_cast<uint32_t>(value),sign=(b>>16)&0x8000u,m=b&0x7fffffu;int e=static_cast<int>((b>>23)&255)-112;uint16_t h;
 if(e>=31)h=static_cast<uint16_t>(sign|0x7c00u);else if(e<=0)h=static_cast<uint16_t>(e< -10?sign:sign|((m|0x800000u)>>(14-e)));else h=static_cast<uint16_t>(sign|(static_cast<uint32_t>(e)<<10)|(m>>13));return num::f16ToF32(h);
}
inline std::vector<uint8_t> compose(const Header& h,std::span<const uint8_t> rgba,std::span<const float> head,uint32_t pitch){
 validate(h);if(rgba.size()!=h.bytes||pitch<h.width||head.size()<static_cast<size_t>(pitch)*h.height*4)throw std::runtime_error("Invalid composition geometry.");
 auto out=std::vector<uint8_t>(rgba.begin(),rgba.end());
 for(uint32_t y=0;y<h.height;++y)for(uint32_t x=0;x<h.width;++x){auto src=(static_cast<size_t>(y)*h.width+x)*4,idx=(static_cast<size_t>(y)*pitch+x)*4;
  for(int c=0;c<4;++c)if(!std::isfinite(head[idx+c]))throw std::runtime_error("Non-finite neural output; original retained.");
  if(h.blend==0)continue;
  for(int c=0;c<3;++c){float original=rgba[src+c]/255.f;float centred=num::roundF16(num::roundF16(num::roundF16(original)-.5f)*.125f);
   float neural=halfTruncate(std::clamp(std::fma(head[idx+c],.03125f,centred)*8.f+.5f,0.f,1.f));
   out[src+c]=static_cast<uint8_t>(std::clamp(std::lround((original+(neural-original)*h.blend)*255.f),0l,255l));}
 }
 return out;
}
}
