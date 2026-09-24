// SPDX-License-Identifier: MIT
#include "../backend.hpp"
#include <algorithm>
#include <cstdlib>
#ifndef SHINY_SPATIAL_REVISION
#error A pinned backend revision is required
#endif
static_assert(SHINY_SPATIAL_REVISION==1||SHINY_SPATIAL_REVISION==2||SHINY_SPATIAL_REVISION==3);
SHINY_EXPORT std::uint32_t shinyEnhancementAbi(){return 1;}
SHINY_EXPORT std::uint32_t shinyEnhancementRevision(){return SHINY_SPATIAL_REVISION;}
SHINY_EXPORT int shinyEnhance(const std::uint8_t* input,std::uint8_t* output,std::uint32_t width,std::uint32_t height){
 if(!input||!output||input==output||!width||!height||width>960||height>960||std::uint64_t(width)*height>maxFramePixels)return 1;
#if SHINY_SPATIAL_REVISION == 3
 // SDR encoded-luma detail, not a neural model. Common RGB offsets preserve
 // chroma differences, and local/gamut bounds prevent new extrema or clipping.
 auto luma=[&](std::uint32_t i){return (54*int(input[i])+183*int(input[i+1])+19*int(input[i+2])+128)/256;};
 for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x){
  const auto i=(y*width+x)*4;const int center=luma(i);int low=center,high=center,blur=0;
  bool opaque=input[i+3]==255;
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   const auto sx=static_cast<std::uint32_t>(std::clamp(int(x)+dx,0,int(width)-1));
   const auto sy=static_cast<std::uint32_t>(std::clamp(int(y)+dy,0,int(height)-1));
   const auto j=(sy*width+sx)*4;const int light=luma(j);
   low=std::min(low,light);high=std::max(high,light);opaque=opaque&&input[j+3]==255;
   blur+=light*(dx==0?2:1)*(dy==0?2:1);
  }
  const int residual=center-(blur+8)/16;
  int delta=opaque?std::min(12,std::max(0,std::abs(residual)-2)/2)*(residual<0?-1:1):0;
  const int rgbLow=std::min({int(input[i]),int(input[i+1]),int(input[i+2])});
  const int rgbHigh=std::max({int(input[i]),int(input[i+1]),int(input[i+2])});
  delta=std::clamp(delta,std::max(low-center,-rgbLow),std::min(high-center,255-rgbHigh));
  for(unsigned c=0;c<3;++c)output[i+c]=static_cast<std::uint8_t>(int(input[i+c])+delta);
  output[i+3]=input[i+3];
 }
 return 0;
#else
 const int divisor=SHINY_SPATIAL_REVISION==1?8:4;
 for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x){
  auto pixel=(y*width+x)*4;
  auto left=(y*width+(x?x-1:0))*4,right=(y*width+std::min(width-1,x+1))*4;
  auto up=((y?y-1:0)*width+x)*4,down=(std::min(height-1,y+1)*width+x)*4;
  for(std::uint32_t c=0;c<3;++c){int center=input[pixel+c];int residual=center*4-input[left+c]-input[right+c]-input[up+c]-input[down+c];output[pixel+c]=static_cast<std::uint8_t>(std::clamp(center+residual/divisor,0,255));}
  output[pixel+3]=input[pixel+3];
 }return 0;
#endif
}
