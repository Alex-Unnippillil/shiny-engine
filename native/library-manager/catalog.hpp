// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace shiny::libraries {
// Names identify inspection slots, never authorize execution or redistribution.
enum class Family { vlc, dlssSr, dlssFg, dlssRr, streamline };
struct Slot { std::string_view path; Family family; bool dll; bool required; };
inline constexpr std::array<Slot, 12> slots{{
    {"vlc.exe", Family::vlc, false, true},
    {"libvlc.dll", Family::vlc, true, true},
    {"libvlccore.dll", Family::vlc, true, true},
    {"plugins/video_output/libdirect3d11_plugin.dll", Family::vlc, true, false},
    {"nvngx_dlss.dll", Family::dlssSr, true, false},
    {"nvngx_dlssg.dll", Family::dlssFg, true, false},
    {"nvngx_dlssd.dll", Family::dlssRr, true, false},
    {"sl.interposer.dll", Family::streamline, true, false},
    {"sl.common.dll", Family::streamline, true, false},
    {"sl.dlss.dll", Family::streamline, true, false},
    {"sl.dlss_g.dll", Family::streamline, true, false},
    {"sl.dlss_d.dll", Family::streamline, true, false},
}};
inline std::string lowerAscii(std::string value) {
    for (auto& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
inline const Slot* candidateSlot(std::string_view filename) {
    auto name = lowerAscii(std::string(filename));
    for (const auto& slot : slots) {
        auto last = slot.path.find_last_of('/');
        auto base = last == std::string_view::npos ? slot.path : slot.path.substr(last + 1);
        if (name == base) return &slot;
    }
    return nullptr;
}
inline std::string_view familyName(Family family) {
    switch (family) {
        case Family::vlc: return "vlc-runtime";
        case Family::dlssSr: return "dlss-super-resolution";
        case Family::dlssFg: return "dlss-frame-generation";
        case Family::dlssRr: return "dlss-ray-reconstruction";
        case Family::streamline: return "streamline";
    }
    return "unknown";
}
inline std::string_view blockedReason(Family family) {
    return family == Family::vlc ? "vlc-requires-complete-verified-runtime-bundle"
                                 : "no-supported-vlc-dlss-library-adapter";
}
struct PeInfo { std::string_view architecture; bool dll; };
// Bounded metadata parser, NOT a Windows loader or comprehensive executable validator.
inline PeInfo inspectPe(std::span<const std::uint8_t> bytes) {
    auto need = [&](std::size_t at, std::size_t length) {
        if (at > bytes.size() || length > bytes.size() - at)
            throw std::runtime_error("truncated-pe");
    };
    auto u16 = [&](std::size_t at) -> std::uint16_t {
        need(at, 2); return static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << 8));
    };
    auto u32 = [&](std::size_t at) -> std::uint32_t {
        need(at, 4); return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at+1]) << 8)
            | (std::uint32_t(bytes[at+2]) << 16) | (std::uint32_t(bytes[at+3]) << 24);
    };
    need(0, 64);
    if (u16(0) != 0x5a4d) throw std::runtime_error("not-pe");
    const std::size_t pe = u32(0x3c);
    if (pe < 64) throw std::runtime_error("invalid-pe-offset");
    need(pe, 24);
    if (u32(pe) != 0x00004550) throw std::runtime_error("not-pe");
    const auto machine = u16(pe + 4), sections = u16(pe + 6);
    const auto optionalSize = u16(pe + 20), flags = u16(pe + 22);
    if (!(flags & 2) || !sections || sections > 96) throw std::runtime_error("invalid-pe-header");
    const std::size_t optional = pe + 24;
    need(optional, optionalSize);
    const auto magic = u16(optional);
    const bool pe64 = magic == 0x20b;
    if ((!pe64 && magic != 0x10b) || optionalSize < (pe64 ? 112 : 96))
        throw std::runtime_error("invalid-optional-header");
    const std::size_t directories = pe64 ? 112 : 96;
    if (u32(optional + directories - 4) > (optionalSize - directories) / 8)
        throw std::runtime_error("invalid-data-directories");
    std::string_view arch;
    if (machine == 0x8664 && pe64) arch = "x64";
    else if (machine == 0x14c && !pe64) arch = "x86";
    else if (machine == 0xaa64 && pe64) arch = "arm64";
    else throw std::runtime_error("unsupported-pe-machine");
    const std::size_t table = optional + optionalSize;
    need(table, std::size_t(sections) * 40);
    const auto headers = u32(optional + 60);
    if (headers < table + std::size_t(sections) * 40 || headers > bytes.size())
        throw std::runtime_error("invalid-header-size");
    for (std::size_t i = 0; i < sections; ++i) {
        const std::size_t row = table + i * 40;
        const auto size = u32(row + 16), offset = u32(row + 20);
        if (size) {
            if (offset < headers) throw std::runtime_error("invalid-section-offset");
            need(offset, size);
        }
    }
    return {arch, (flags & 0x2000) != 0};
}
inline std::string jsonString(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
}
