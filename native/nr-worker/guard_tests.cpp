// SPDX-License-Identifier: MIT
#include "model_guard.hpp"
#include "test_fixture.hpp"
#include <iostream>
using namespace shiny::nrpolicy;
int main(){int checks=0;auto root=std::filesystem::temp_directory_path()/(L"Shiny-model-test-"+std::to_wstring(GetCurrentProcessId()));auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("Model guard assertion "+std::to_string(checks));};auto bad=[&](auto f){bool threw=false;try{f();}catch(...){threw=true;}check(threw);};try{
 std::filesystem::create_directories(root/L"model");auto text=testing::fixture();auto layout=validateLayout(text);std::vector<uint8_t> zero(static_cast<size_t>(layout.bytes),0);auto digest=lowerHash(sha256Hex(zero.data(),zero.size()));text.replace(text.find(std::string(64,'0')),64,digest);
 {std::ofstream data(root/L"model"/L"stage.bin",std::ios::binary);data.write(reinterpret_cast<const char*>(zero.data()),zero.size());check(data.good());}
 auto write=[&]{std::ofstream out(root/L"manifest.json",std::ios::binary);out<<text;};write();const auto identity=fingerprint(root);
 {ModelGuard inspected(root,ModelUse::Inspect);check(inspected.digest==identity);check(!inspected.reviewed);check(inspected.layout.bytes==zero.size());}
 bad([&]{ModelGuard denied(root);});bad([&]{ModelGuard denied(root,ModelUse::LocalResearch,std::string(64,'f'));});
 {ModelGuard research(root,ModelUse::LocalResearch,identity);check(!research.reviewed);HANDLE change=CreateFileW((root/L"model"/L"stage.bin").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);check(change==INVALID_HANDLE_VALUE);if(change!=INVALID_HANDLE_VALUE)CloseHandle(change);HANDLE meta=CreateFileW((root/L"manifest.json").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);check(meta==INVALID_HANDLE_VALUE);if(meta!=INVALID_HANDLE_VALUE)CloseHandle(meta);}
 text+=' ';write();bad([&]{ModelGuard denied(root,ModelUse::LocalResearch,identity);});
 {std::fstream data(root/L"model"/L"stage.bin",std::ios::binary|std::ios::in|std::ios::out);data.put(1);}bad([&]{ModelGuard denied(root,ModelUse::Inspect);});
 std::filesystem::remove_all(root);std::cout<<checks<<" real file-lock, explicit consent, TOCTOU and stage-hash checks passed. Synthetic zero data, no GPU inference.\n";return 0;
 }catch(const std::exception& e){std::filesystem::remove_all(root);std::cerr<<e.what();return 1;}}
