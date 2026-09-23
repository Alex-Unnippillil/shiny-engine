// SPDX-License-Identifier: MIT
#pragma once
#include "approvals.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
namespace shiny::nrpolicy {
enum class Access { Reviewed, LocalResearch };
inline bool hashText(std::string_view s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string_view::npos;}
// This is explicit user intent, not licensing verification or a security boundary.
// Never append a user-supplied identity to the curated approval registry.
inline bool authorize(Access mode,std::string_view actual,std::string_view consented={}){
 if(!hashText(actual))throw std::runtime_error("Invalid manifest fingerprint.");
 if(mode==Access::Reviewed){if(!approved(actual))throw std::runtime_error("MODEL_NOT_REVIEWED: choose Local research and acknowledge authorized use to test a local model.");return true;}
 if(mode!=Access::LocalResearch)throw std::runtime_error("Unknown model access mode.");
 if(!hashText(consented)||actual!=consented)throw std::runtime_error("MODEL_CHANGED: inspect and acknowledge this exact manifest again.");
 return false;
}
inline constexpr std::string_view researchNotice="LOCAL RESEARCH / UNVERIFIED — user-supplied model; no vendor parity, temporal quality or real-time certification.";
}
