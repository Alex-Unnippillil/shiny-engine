// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <string_view>
namespace shiny::nrpolicy {
struct Approval {
 std::string_view manifestSha256, review, runtimeUseEvidence, nativeParityEvidence;
};
// Deliberately empty. Add an exact identity only after independent model runtime-use
// review and native preprocessing/head/output fixtures, not a user file selection.
inline constexpr std::array<Approval,0> reviewed{};
inline bool approved(std::string_view digest){
 for(const auto& p:reviewed)if(p.manifestSha256==digest&&!p.review.empty()&&!p.runtimeUseEvidence.empty()&&!p.nativeParityEvidence.empty())return true;
 return false;
}
}
