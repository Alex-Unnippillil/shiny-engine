// SPDX-License-Identifier: MIT
#pragma once
#include "backend.hpp"
#include <array>
#include <stdexcept>
namespace shiny::packages::wire {
inline constexpr std::uint32_t magic=0x534c504b,abi=1;
enum Command:std::uint32_t{Frame=1,Result=2,Reset=3,Ack=4};
struct Greeting {std::uint32_t magicValue=magic,abiValue=abi,revision=0,format=1;
std::array<char,64> package{};
};
struct FrameHeader {
 std::uint32_t magicValue=magic,abiValue=abi,command=Frame,width=0,height=0,bytes=0;
 std::uint64_t sequence=0,generation=0;
std::int64_t ptsMs=0;
};
static_assert(sizeof(Greeting)==80&&sizeof(FrameHeader)==48);
inline void validate(const FrameHeader& h,bool reply=false){
 if(h.magicValue!=magic||h.abiValue!=abi||!h.sequence||h.ptsMs<0)throw std::runtime_error("invalid-frame-header");
 if(h.command==(reply?Ack:Reset)){
  if(h.width||h.height||h.bytes)throw std::runtime_error("invalid-reset-frame");
return;
 }
 if(h.command!=(reply?Result:Frame)||!h.width||!h.height||h.width>960||h.height>960||std::uint64_t(h.width)*h.height>maxFramePixels||h.bytes!=h.width*h.height*4)throw std::runtime_error("invalid-frame-shape");
}
}
