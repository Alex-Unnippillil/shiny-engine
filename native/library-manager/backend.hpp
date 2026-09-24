// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#ifdef _WIN32
#define SHINY_EXPORT extern "C" __declspec(dllexport)
#else
#define SHINY_EXPORT extern "C"
#endif
// ABI 1 is deliberately small: tightly packed RGBA8, no retained buffers or callbacks.
// Spatial reference processing only. No neural model, DLSS, temporal state or HDR.
using AbiFn=std::uint32_t(*)();
using ProcessFn=int(*)(const std::uint8_t*,std::uint8_t*,std::uint32_t,std::uint32_t);
inline constexpr std::uint32_t maxFramePixels=960*540;
