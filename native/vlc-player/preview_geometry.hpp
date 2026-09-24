// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace shiny::preview {
struct Rect {float x=0,y=0,w=0,h=0;};
inline Rect fit(Rect area,unsigned width,unsigned height,bool actual=false){
    if(!width||!height||area.w<=0||area.h<=0)return {};
    float scale=actual?1.f:std::min(area.w/static_cast<float>(width),area.h/static_cast<float>(height));
    float w=static_cast<float>(width)*scale,h=static_cast<float>(height)*scale;
    return {area.x+(area.w-w)*.5f,area.y+(area.h-h)*.5f,w,h};
}
// Managed ABI budget. Neural workbench keeps its separate 512-pixel default.
inline std::array<unsigned,2> decodeSize(unsigned w,unsigned h,unsigned edge=960){
    if(!w||!h||w>16384||h>16384||edge<33||edge>960)throw std::runtime_error("invalid-preview-dimensions");
    double scale=std::min({1.,double(edge)/std::max(w,h),std::sqrt(518400./(double(w)*h))});
    auto x=static_cast<unsigned>(std::floor(w*scale)),y=static_cast<unsigned>(std::floor(h*scale));
    if(x<33||y<33)throw std::runtime_error("preview-aspect-ratio-too-extreme");
    return {x,y};
}
inline float wipe(float value){return std::isfinite(value)?std::clamp(value,0.f,1.f):.5f;}
// A bounded rolling latency window; values are worker round-trip, never GPU timing.
class Latency {
    std::array<double,120> values{};std::size_t count=0,next=0;
public:
    void add(double value){if(!std::isfinite(value)||value<0)return;values[next]=value;next=(next+1)%values.size();count=std::min(count+1,values.size());}
    void clear(){count=next=0;}
    double p95()const{if(!count)return 0;auto sorted=values;std::sort(sorted.begin(),sorted.begin()+static_cast<std::ptrdiff_t>(count));return sorted[(count*95+99)/100-1];}
    std::size_t size()const{return count;}
};
}
