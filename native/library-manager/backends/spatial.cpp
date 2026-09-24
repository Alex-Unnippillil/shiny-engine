// SPDX-License-Identifier: MIT
#include "../backend.hpp"
#include <algorithm>
#ifndef SHINY_SPATIAL_REVISION
#error A pinned backend revision is required
#endif
static_assert(SHINY_SPATIAL_REVISION==1||SHINY_SPATIAL_REVISION==2);
SHINY_EXPORT std::uint32_t shinyEnhancementAbi(){return 1;}
SHINY_EXPORT std::uint32_t shinyEnhancementRevision(){return SHINY_SPATIAL_REVISION;}
SHINY_EXPORT int shinyEnhance(const std::uint8_t* input,std::uint8_t* output,std::uint32_t width,std::uint32_t height){
 if(!input||!output||input==output||!width||!height||width>960||height>960||std::uint64_t(width)*height>maxFramePixels)return 1;
 const int divisor=SHINY_SPATIAL_REVISION==1?8:4;
 for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x){
  auto pixel=(y*width+x)*4;
  auto left=(y*width+(x?x-1:0))*4,right=(y*width+std::min(width-1,x+1))*4;
  auto up=((y?y-1:0)*width+x)*4,down=(std::min(height-1,y+1)*width+x)*4;
  for(std::uint32_t c=0;c<3;++c){int center=input[pixel+c];int residual=center*4-input[left+c]-input[right+c]-input[up+c]-input[down+c];output[pixel+c]=static_cast<std::uint8_t>(std::clamp(center+residual/divisor,0,255));}
  output[pixel+3]=input[pixel+3];
 }return 0;
}
