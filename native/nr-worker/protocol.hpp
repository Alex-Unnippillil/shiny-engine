// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <span>
namespace shiny::nrwire {
// Private anonymous pipes only. No browser/native messaging or network listener.
inline constexpr uint32_t magic=0x36524e53, version=1, maxEdge=512, maxBytes=maxEdge*maxEdge*4;
enum Command:uint32_t { Frame=1, Stop=2, Ready=100, Result=101, Error=102 };
struct Header {
 uint32_t signature=magic, api=version, command=Frame, sequence=0, width=0, height=0, bytes=0, reserved=0;
 float tone=.5f, structure=.5f, blend=1.f, split=.5f;
 std::array<uint32_t,4> padding{};
};
static_assert(sizeof(Header)==64);
inline void validate(const Header& h) {
 if(h.signature!=magic||h.api!=version||h.reserved||std::any_of(h.padding.begin(),h.padding.end(),[](auto n){return n!=0;}))throw std::runtime_error("Invalid NR protocol header.");
 for(float f:{h.tone,h.structure,h.blend,h.split})if(!std::isfinite(f)||f<0||f>1)throw std::runtime_error("Invalid neural controls.");
 if(h.command==Stop){if(h.bytes||h.width||h.height)throw std::runtime_error("Invalid stop message.");return;}
 if(h.command!=Frame||h.width<33||h.height<33||h.width>maxEdge||h.height>maxEdge||h.bytes!=h.width*h.height*4)throw std::runtime_error("NR preview requires 33–512 pixel SDR RGBA frames.");
}
inline void validateReply(const Header& h,const Header* request=nullptr) {
 if(h.signature!=magic||h.api!=version||h.bytes>maxBytes)throw std::runtime_error("Invalid NR worker response.");
 if(h.command==Ready&&request)throw std::runtime_error("Unexpected repeated worker greeting.");
 if(h.command==Error||h.command==Ready){if(h.bytes>4096)throw std::runtime_error("Oversized status response.");return;}
 if(!request||h.command!=Result||h.sequence!=request->sequence||h.width!=request->width||h.height!=request->height||h.bytes!=request->bytes)throw std::runtime_error("Mismatched NR frame response.");
}
}
