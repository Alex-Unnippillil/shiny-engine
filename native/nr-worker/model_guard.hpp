// SPDX-License-Identifier: MIT
#pragma once
#include "manifest_schema.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include "sha256.h"
namespace shiny::nrpolicy {
inline std::string lowerHash(std::string s){return lowerAscii(std::move(s));}
inline std::string readManifest(const std::filesystem::path& root){
 auto p=root/L"manifest.json";auto n=std::filesystem::file_size(p);
 if(!n||n>2*1024*1024)throw std::runtime_error("Model manifest must be between 1 byte and 2 MiB.");
 std::ifstream in(p,std::ios::binary);std::string text(static_cast<size_t>(n),'\0');
 if(!in.read(text.data(),static_cast<std::streamsize>(text.size())))throw std::runtime_error("Model manifest cannot be read.");return text;
}
inline std::string fingerprint(const std::filesystem::path& root){auto text=readManifest(root);return lowerHash(sha256Hex(reinterpret_cast<const uint8_t*>(text.data()),text.size()));}
class ModelGuard {
 std::vector<HANDLE> locks;std::filesystem::path root;
 std::set<std::wstring> lockedDirectories;
 void hold(const std::filesystem::path& path,bool directory){
  DWORD flags=FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:FILE_ATTRIBUTE_NORMAL);
  DWORD share=FILE_SHARE_READ|(directory?FILE_SHARE_WRITE:0);
  HANDLE h=CreateFileW(path.c_str(),directory?FILE_READ_ATTRIBUTES:GENERIC_READ,share,nullptr,OPEN_EXISTING,flags,nullptr);
  if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot lock model data or its directory against replacement.");
  FILE_ATTRIBUTE_TAG_INFO info{};
  bool valid=GetFileInformationByHandleEx(h,FileAttributeTagInfo,&info,sizeof info)&&!(info.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&bool(info.FileAttributes&FILE_ATTRIBUTE_DIRECTORY)==directory;
  if(!valid){CloseHandle(h);throw std::runtime_error("Model paths must be ordinary directories and files, not reparse links.");}
  locks.push_back(h);
 }
 void lock(const std::filesystem::path& path){
  // Hold ancestor directory handles without FILE_SHARE_DELETE as well as data
  // files. This closes the directory-rename gap in pathname-based double reads.
  auto current=path.root_path();
  if(lockedDirectories.insert(current.wstring()).second)hold(current,true);
  auto relative=path.lexically_relative(current);size_t count=0;
  for(const auto& segment:relative){(void)segment;++count;}
  size_t index=0;for(const auto& segment:relative){current/=segment;
   if(++index<count){if(lockedDirectories.insert(current.wstring()).second)hold(current,true);}
   else hold(current,false);
  }
 }

 public:
 std::string digest;bool reviewed=false;ManifestSpec manifest;
 explicit ModelGuard(const std::filesystem::path& directory,Access access=Access::Reviewed,std::string_view consented={}):root(std::filesystem::canonical(directory)){
  try{
   // Read locks remain held until the graph and all its weight buffers are destroyed.
   lock(root/L"manifest.json");auto text=readManifest(root);digest=lowerHash(sha256Hex(reinterpret_cast<const uint8_t*>(text.data()),text.size()));
   reviewed=authorize(access,digest,consented);
   // No user-controlled JSON reaches the upstream reader before strict validation.
   manifest=parseManifest(text);
   for(const auto& stage:manifest.stages){
    auto file=root/L"model"/std::filesystem::path(stage.file);lock(file);
    if(std::filesystem::file_size(file)!=stage.bytes)throw std::runtime_error("Model stage size mismatch.");
    std::ifstream f(file,std::ios::binary);std::vector<uint8_t> bytes(static_cast<size_t>(stage.bytes));
    if(!f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("Cannot read locked stage.");
    if(lowerHash(sha256Hex(bytes.data(),bytes.size()))!=stage.digest)throw std::runtime_error("Model stage hash mismatch.");
   }
  }catch(...){for(auto h:locks)CloseHandle(h);locks.clear();throw;}
 }
 const std::filesystem::path& directory() const{return root;}
 ~ModelGuard(){for(auto h:locks)CloseHandle(h);}
 ModelGuard(const ModelGuard&)=delete;ModelGuard& operator=(const ModelGuard&)=delete;
};
}
