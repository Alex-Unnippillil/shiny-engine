// SPDX-License-Identifier: MIT
#pragma once
#include "approvals.hpp"
#include "manifest.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include "sha256.h"
namespace shiny::nrpolicy {
inline std::vector<uint8_t> readBounded(const std::filesystem::path& path,uint64_t maximum){
 auto n=std::filesystem::file_size(path);if(!n||n>maximum)throw std::runtime_error("Model file is empty or exceeds its byte limit.");
 std::ifstream in(path,std::ios::binary);std::vector<uint8_t> bytes(static_cast<size_t>(n));
 if(!in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(n)))throw std::runtime_error("Cannot read model file.");return bytes;
}
inline std::string fingerprint(const std::filesystem::path& root){auto b=readBounded(root/L"manifest.json",2*1024*1024);return lowerHash(sha256Hex(b.data(),b.size()));}
enum class ModelUse { Inspect, Reviewed, LocalResearch };
class ModelGuard {
 std::vector<HANDLE> locks;std::set<std::filesystem::path> locked;std::filesystem::path root;
 void hold(const std::filesystem::path& path,bool directory){
  if(locked.count(path))return;
  HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:0),nullptr);
  if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot lock local model files for reading.");
  BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||static_cast<bool>(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory){CloseHandle(h);throw std::runtime_error("Model paths must be ordinary files/directories, not links.");}
  locks.push_back(h);locked.insert(path);
 }
 void holdStage(const std::filesystem::path& relative){auto part=root/L"model";hold(part,true);auto parent=relative.parent_path();for(const auto& s:parent){part/=s;hold(part,true);}hold(root/L"model"/relative,false);}
 public:
 std::string digest;Layout layout;bool reviewed=false;
 explicit ModelGuard(const std::filesystem::path& directory,ModelUse use=ModelUse::Reviewed,const std::string& consentDigest={}):root(std::filesystem::canonical(directory)){
  try{
   hold(root,true);hold(root/L"manifest.json",false);auto bytes=readBounded(root/L"manifest.json",2*1024*1024);digest=lowerHash(sha256Hex(bytes.data(),bytes.size()));reviewed=approved(digest);
   if(use==ModelUse::Reviewed&&!reviewed)throw std::runtime_error("MODEL_NOT_REVIEWED: use explicit local research consent for an unreviewed model.");
   if(use==ModelUse::LocalResearch&&(!hashString(consentDigest)||lowerHash(consentDigest)!=digest))throw std::runtime_error("RESEARCH_IDENTITY_CHANGED: reselect the model and confirm this exact fingerprint.");
   layout=validateLayout(std::string(bytes.begin(),bytes.end()));
   for(const auto& s:layout.stages){auto relative=std::filesystem::path(s.file);holdStage(relative);auto path=root/L"model"/relative;
    if(std::filesystem::file_size(path)!=s.bytes)throw std::runtime_error("Stage size differs from manifest.");auto data=readBounded(path,256ull*1024*1024);
    if(lowerHash(sha256Hex(data.data(),data.size()))!=lowerHash(s.hash))throw std::runtime_error("Model stage SHA-256 mismatch.");
   }
  }catch(...){for(auto h:locks)CloseHandle(h);locks.clear();throw;}
 }
 ~ModelGuard(){for(auto h:locks)CloseHandle(h);}
 ModelGuard(const ModelGuard&)=delete;ModelGuard& operator=(const ModelGuard&)=delete;
};
}
