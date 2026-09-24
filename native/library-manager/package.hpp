// SPDX-License-Identifier: MIT
#pragma once
#include "database.hpp"
#include <functional>
namespace shiny::packages {
inline constexpr std::size_t maxComponent=32*1024*1024,maxPackage=128*1024*1024,maxManifest=32768;
struct Component {std::string name,sha256;std::uint64_t size=0;};
struct Manifest {
 std::string raw,sha256,id,version,family,architecture,license,source,dependencySet;
 int abi=0;std::vector<Component> files;
};
Manifest parseManifest(std::string raw);
struct Policy {
 std::vector<std::string> approvedDigests;
 bool approved(const Manifest& manifest)const;
 std::string blockReason(const Manifest& manifest)const;
};
struct Snapshot {
 DirectoryPin directory;std::vector<FilePin> pins;Manifest manifest;std::vector<Bytes> contents;
 Snapshot(const fs::path& folder,std::stop_token stop={});
};
std::string quarantineManifest(std::string filename,const Bytes& bytes);
}
