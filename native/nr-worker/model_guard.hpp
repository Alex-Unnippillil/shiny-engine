// SPDX-License-Identifier: MIT
#pragma once
#include "approvals.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <set>
#include <cctype>
#include "sha256.h"
#include "json.h"
namespace shiny::nrpolicy {
inline std::string lowerHash(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline std::string fingerprint(const std::filesystem::path& root){
 auto p=root/L"manifest.json";auto n=std::filesystem::file_size(p);
 if(!n||n>2*1024*1024)throw std::runtime_error("Model manifest must be between 1 byte and 2 MiB.");
 std::ifstream in(p,std::ios::binary);std::vector<uint8_t> bytes(static_cast<size_t>(n));
 if(!in.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("Model manifest cannot be read.");
 return lowerHash(sha256Hex(bytes.data(),bytes.size()));
}
class ModelGuard {
 std::vector<HANDLE> locks;
 std::filesystem::path root;
 void lock(const std::filesystem::path& path){
  // Hold read-only sharing until model destruction; disallow nested reparse links.
  auto rel=path.lexically_relative(root);auto part=root;
  for(const auto& segment:rel){part/=segment;DWORD a=GetFileAttributesW(part.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Model files must not be reparse links.");}
  HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot acquire read-only model lock.");locks.push_back(h);
 }
 public:
 std::string digest;
 explicit ModelGuard(const std::filesystem::path& directory):root(std::filesystem::canonical(directory)){
  try{
   lock(root/L"manifest.json");digest=fingerprint(root);
   if(!approved(digest))throw std::runtime_error("MODEL_NOT_REVIEWED: no native runtime-use and correctness approval for SHA-256 "+digest);
   std::ifstream in(root/L"manifest.json",std::ios::binary);std::string text((std::istreambuf_iterator<char>(in)),{});
   auto manifest=json::parse(text); // Only exact reviewed bytes reach the upstream parser.
   if(manifest["totals"]["blockCount"].integer()!=71 || manifest["stages"].size()>256)throw std::runtime_error("Unsupported native graph layout.");
   size_t total=0;std::set<std::string> seen;
   for(const auto& s:manifest["stages"].array){
    const auto name=s["file"].str();std::filesystem::path relative(name);
    if(name.empty()||name.size()>240||relative.is_absolute()||name.find(':')!=std::string::npos||name.find('\\')!=std::string::npos||relative.extension()!=L".bin")throw std::runtime_error("Unsafe model stage path.");
    for(const auto& part:relative)if(part==L".."||part==L"."||part.empty())throw std::runtime_error("Unsafe model stage path.");
    if(!seen.insert(lowerHash(name)).second)throw std::runtime_error("Duplicate model path.");
    auto file=root/L"model"/relative;lock(file);auto n=std::filesystem::file_size(file);total+=static_cast<size_t>(n);
    if(!n||n>256*1024*1024||total>256*1024*1024||n!=static_cast<uint64_t>(s["packedByteLength"].integer()))throw std::runtime_error("Model size mismatch.");
    std::ifstream f(file,std::ios::binary);std::vector<uint8_t> bytes(static_cast<size_t>(n));if(!f.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("Cannot read locked stage.");
    if(lowerHash(sha256Hex(bytes.data(),bytes.size()))!=lowerHash(s["sha256"].str()))throw std::runtime_error("Model stage hash mismatch.");
   }
  }catch(...){for(auto h:locks)CloseHandle(h);locks.clear();throw;}
 }
 ~ModelGuard(){for(auto h:locks)CloseHandle(h);}
 ModelGuard(const ModelGuard&)=delete;ModelGuard& operator=(const ModelGuard&)=delete;
};
}
