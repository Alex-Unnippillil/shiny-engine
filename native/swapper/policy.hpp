// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
namespace shiny::swapper {
inline constexpr std::uint64_t MaxBytes = 512ull * 1024 * 1024;
// Filename recognition is a UX guard, NEVER publisher approval or proof of safety.
enum class Tool { dlss5, classic };
inline std::wstring lower(std::wstring name) {
    for (auto& c : name) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    return name;
}
inline Tool recognize(std::wstring_view filename) {
    const auto n = lower(std::wstring(filename));
    if (n == L"dlss 5 swapper.exe" || n == L"dlss5-swapper.exe") return Tool::dlss5;
    if (n == L"dlss swapper.exe" || n == L"dlss-swapper.exe") return Tool::classic;
    constexpr std::wstring_view prefix = L"dlss5-swapper-", suffix = L"-portable.exe";
    if (n.starts_with(prefix) && n.ends_with(suffix) && n.size() >= prefix.size()+suffix.size()+5 && n.size() <= 64) {
        auto version = std::wstring_view(n).substr(prefix.size(), n.size()-prefix.size()-suffix.size());
        unsigned groups = 1, digits = 0;
        for (auto c : version) {
            if (c >= L'0' && c <= L'9') { if (++digits > 6) throw std::runtime_error("unrecognized-swapper-name"); }
            else if (c == L'.' && digits) { ++groups; digits = 0; }
            else throw std::runtime_error("unrecognized-swapper-name");
        }
        if (groups == 3 && digits) return Tool::dlss5;
    }
    throw std::runtime_error("unrecognized-swapper-name");
}
inline const wchar_t* title(Tool tool) { return tool == Tool::dlss5 ? L"DLSS 5 Swapper (community)" : L"DLSS Swapper (classic)"; }
inline bool localSyntax(std::wstring_view path) {
    if (path.size() < 4 || path.size() > 32760 || !((path[0]>=L'A'&&path[0]<=L'Z')||(path[0]>=L'a'&&path[0]<=L'z')) || path[1]!=L':' || (path[2]!=L'\\'&&path[2]!=L'/')) return false;
    std::size_t start=3;
    for (std::size_t i=3;i<=path.size();++i) {
        if(i!=path.size() && path[i]!=L'\\' && path[i]!=L'/') {
            if (path[i]<32 || std::wstring_view(L":*?\"<>|").find(path[i])!=std::wstring_view::npos) return false;
            continue;
        }
        const auto part=path.substr(start,i-start);
        if (part.empty() || part==L"." || part==L".." || part.back()==L'.' || part.back()==L' ') return false;
        start=i+1;
    }
    return true;
}
inline std::string_view executableHeader(std::span<const std::uint8_t> b, std::uint64_t fileBytes) {
    auto u16=[&](std::size_t o)->unsigned {
        if(o>b.size() || b.size()-o<2) throw std::runtime_error("truncated-executable-header");
        return unsigned(b[o]) | (unsigned(b[o+1])<<8);
    };
    auto u32=[&](std::size_t o)->std::uint32_t { return u16(o) | (std::uint32_t(u16(o+2))<<16); };
    if (!fileBytes || fileBytes>MaxBytes || b.size()<64 || u16(0)!=0x5a4d) throw std::runtime_error("not-a-supported-executable");
    const auto pe=std::size_t(u32(60));
    if(pe<64 || pe>b.size() || b.size()-pe<26 || u32(pe)!=0x4550)
        throw std::runtime_error("windows-executable-required");
    // A portable wrapper's architecture can differ from its contained application.
    // Both ordinary x86 and x64 processes are valid external tools on this x64 host.
    const auto machine=u16(pe+4), magic=u16(pe+24);
    const bool x64=machine==0x8664 && magic==0x20b;
    const bool x86=machine==0x14c && magic==0x10b;
    if(!x64 && !x86)throw std::runtime_error("windows-x86-or-x64-executable-required");
    const auto flags=u16(pe+22), sections=u16(pe+6), optional=u16(pe+20);
    if (!(flags&2) || (flags&0x2000) || !sections || sections>96 || optional<(x64?112u:96u) || pe+24+optional+sections*40>fileBytes)
        throw std::runtime_error("invalid-executable-metadata");
    return x64?"x64":"x86";
}
}
