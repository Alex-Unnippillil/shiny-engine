// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
namespace shiny::packages {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
void cancelled(std::stop_token stop);
std::string digest(std::span<const std::uint8_t> bytes);
std::string digest(std::string_view text);
bool hashId(std::string_view value);
std::string pathText(const fs::path& path);
fs::path textPath(std::string_view value);
void validateLocal(const fs::path& path);
class DirectoryPin {
 struct Impl; std::unique_ptr<Impl> impl;
 public:
 explicit DirectoryPin(const fs::path& path);
 ~DirectoryPin(); DirectoryPin(DirectoryPin&&) noexcept;
 DirectoryPin& operator=(DirectoryPin&&) noexcept;
};
class FilePin {
 struct Impl; std::unique_ptr<Impl> impl;
 public:
 explicit FilePin(const fs::path& path);
 ~FilePin(); FilePin(FilePin&&) noexcept;
 FilePin& operator=(FilePin&&) noexcept;
 Bytes read(std::size_t limit, std::stop_token stop={}) const;
};
class StoreLock {
 struct Impl; std::unique_ptr<Impl> impl;
 public:
 explicit StoreLock(const fs::path& root);
 ~StoreLock();
};
// Multiple worker readers; exclusive manager lease before deletion.
class PackageLease {
 struct Impl; std::unique_ptr<Impl> impl;
 public:
 PackageLease(const fs::path& file, bool exclusive);
 ~PackageLease();
};
void makeDirectory(const fs::path& path);
void writeNew(const fs::path& path,std::span<const std::uint8_t> bytes,std::stop_token stop={});
void writeNew(const fs::path& path,std::string_view text,std::stop_token stop={});
void promote(const fs::path& from,const fs::path& to);
void eraseFlat(const fs::path& directory);
fs::path defaultStore();
fs::path executableDirectory();
std::string hostArchitecture();
}
