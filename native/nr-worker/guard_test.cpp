// SPDX-License-Identifier: MIT
// Real Windows handles and hash validation using generated zero data, NOT trained weights.
#include "model_guard.hpp"
#include <iostream>
#include <sstream>
using namespace shiny::nrpolicy;
int wmain(int argc,wchar_t** argv){
 std::vector<std::string> passed;std::string error;std::filesystem::path root;
 auto require=[&](bool yes,const char* name){if(!yes)throw std::runtime_error(name);passed.emplace_back(name);};
 auto rejects=[&](auto operation,const char* name){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}require(rejected,name);};
 try{
  wchar_t temporary[32768]{};if(!GetTempPathW(32768,temporary))throw std::runtime_error("No temporary directory");
  root=std::filesystem::path(temporary)/(L"Shiny-\u03b4-\u6a21\u578b-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
  std::filesystem::create_directories(root/L"model");
  auto required=requiredTensors();uint64_t total=0;std::ostringstream tensors;bool first=true;
  for(const auto& [name,r]:required){auto dot=name.find('.'),next=name.find('.',dot+1);auto block=name.substr(5,dot-5),layer=name.substr(dot+6,next-dot-6);
   if(!first)tensors<<',';first=false;
   tensors<<"{\"name\":\""<<name<<"\",\"block\":"<<block<<",\"layer\":"<<layer<<",\"parameter\":\"layer\",\"stage\":\"synthetic\",\"stageOffset\":"<<total<<",\"byteLength\":"<<r.minimum<<"}";total+=r.minimum;
  }
  std::string stageHash;
  {std::vector<uint8_t> zeros(static_cast<size_t>(total));stageHash=sha256Hex(zeros.data(),zeros.size());std::ofstream file(root/L"model"/L"stage.bin",std::ios::binary);file.write(reinterpret_cast<const char*>(zeros.data()),zeros.size());require(file.good(),"generated non-trained zero stage written");}
  auto text=std::string("{\"totals\":{\"blockCount\":71},\"stages\":[{\"id\":\"synthetic\",\"file\":\"stage.bin\",\"sha256\":\"")+stageHash+"\",\"packedByteLength\":"+std::to_string(total)+"}],\"tensors\":["+tensors.str()+"]}";
  auto writeManifest=[&](const std::string& value){std::ofstream file(root/L"manifest.json",std::ios::binary);file<<value;if(!file)throw std::runtime_error("Fixture manifest write failed");};writeManifest(text);
  const auto digest=fingerprint(root);const auto stage=root/L"model"/L"stage.bin";
  {ModelGuard guard(root,Access::LocalResearch,digest);
   require(!guard.reviewed&&guard.manifest.tensors.size()==required.size(),"valid local-research intake succeeds without curated approval");
   require(guard.digest==digest&&guard.manifest.bytes==total,"Unicode model directory and exact stage identity validated");
   HANDLE writer=CreateFileW(stage.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
   bool blocked=writer==INVALID_HANDLE_VALUE;if(!blocked)CloseHandle(writer);require(blocked,"held stage rejects concurrent writes");
   require(!MoveFileW((root/L"model").c_str(),(root/L"renamed").c_str()),"held ancestor rejects directory replacement");
  }
  HANDLE writer=CreateFileW(stage.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);require(writer!=INVALID_HANDLE_VALUE,"stage write access returns after guard disposal");if(writer!=INVALID_HANDLE_VALUE)CloseHandle(writer);
  require(MoveFileW((root/L"model").c_str(),(root/L"renamed").c_str())!=0,"directory rename returns after guard disposal");std::filesystem::rename(root/L"renamed",root/L"model");
  rejects([&]{ModelGuard denied(root);},"reviewed-only mode still rejects the generated unreviewed fixture");
  writeManifest(text+" ");rejects([&]{ModelGuard changed(root,Access::LocalResearch,digest);},"changed manifest requires renewed acknowledgment");writeManifest(text);
  {std::fstream file(stage,std::ios::in|std::ios::out|std::ios::binary);file.put('\1');}
  rejects([&]{ModelGuard changed(root,Access::LocalResearch,digest);},"changed stage contents fail the actual SHA-256 check");
  std::filesystem::resize_file(stage,total-1);
  rejects([&]{ModelGuard changed(root,Access::LocalResearch,digest);},"truncated stage fails its declared byte size");
  require(reviewed.empty(),"research intake never mutates curated approvals");
 }catch(const std::exception& ex){error=ex.what();}
 if(!root.empty()){std::error_code ec;std::filesystem::remove_all(root,ec);if(ec&&error.empty())error="Temporary fixture cleanup failed";}
 if(argc==2){std::ofstream out(argv[1]);out<<"{\"passed\":[";for(size_t i=0;i<passed.size();++i)out<<(i?",":"")<<'"'<<passed[i]<<'"';out<<"],\"success\":"<<(error.empty()?"true":"false")<<",\"trainedModelInference\":false,\"scope\":\"Generated zero tensors; real Windows file and directory locks, Unicode paths, exact hashes and corruption rejection. No GPU or trained model.\"}\n";if(!out&&error.empty())error="Report write failed";}
 if(!error.empty()){std::cerr<<error<<'\n';return 1;}std::cout<<passed.size()<<" real Windows data-intake/locking checks passed; no trained inference\n";return 0;
}
