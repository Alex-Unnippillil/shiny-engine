// SPDX-License-Identifier: MIT
#include "store.hpp"
#include "wire.hpp"
#include "bundle_catalog.hpp"
#include <windows.h>
#include <algorithm>
namespace {
void transfer(HANDLE pipe,void* data,std::size_t bytes,bool output){
 auto* p=static_cast<std::uint8_t*>(data);
while(bytes){DWORD n=0;
auto size=static_cast<DWORD>(std::min<std::size_t>(bytes,65536));
  BOOL ok=output?WriteFile(pipe,p,size,&n,nullptr):ReadFile(pipe,p,size,&n,nullptr);
if(!ok||!n)throw std::runtime_error("private-pipe-closed");
p+=n;
bytes-=n;
 }
}
struct Module {HMODULE value=nullptr;
~Module(){if(value)FreeLibrary(value);
}};
}
int wmain(int argc,wchar_t** argv){using namespace shiny::packages;
try{
 if(argc!=5||std::wstring_view(argv[1])!=L"--store-dir"||std::wstring_view(argv[3])!=L"--package")return 2;
 if(!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32))return 2;
 auto id=pathText(fs::path(argv[4]));
VerifiedLease lease(argv[2],id,buildPolicy());
 auto& manifest=lease.snapshot->manifest;
auto dll=fs::path(argv[2])/L"packages"/textPath(id)/L"shiny_spatial.dll";
 Module module;
module.value=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!module.value)return 3;
 auto abi=reinterpret_cast<AbiFn>(GetProcAddress(module.value,"shinyEnhancementAbi"));
 auto revision=reinterpret_cast<AbiFn>(GetProcAddress(module.value,"shinyEnhancementRevision"));
 auto process=reinterpret_cast<ProcessFn>(GetProcAddress(module.value,"shinyEnhance"));
 if(!abi||!revision||!process||abi()!=1)return 3;
 wire::Greeting greeting;
greeting.revision=revision();
 if((manifest.version=="1.0.0"?1u:manifest.version=="1.1.0"?2u:0u)!=greeting.revision)return 3;
 std::copy(id.begin(),id.end(),greeting.package.begin());
auto input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
 if(GetFileType(input)!=FILE_TYPE_PIPE||GetFileType(output)!=FILE_TYPE_PIPE)return 2;
 transfer(output,&greeting,sizeof greeting,true);
std::uint64_t previous=0;
 for(;;){
  wire::FrameHeader h;
transfer(input,&h,sizeof h,false);
wire::validate(h);
if(h.sequence<=previous)return 4;
previous=h.sequence;
  if(h.command==wire::Reset){h.command=wire::Ack;
transfer(output,&h,sizeof h,true);
continue;
}
  Bytes original(h.bytes),enhanced(h.bytes);
transfer(input,original.data(),original.size(),false);
  if(process(original.data(),enhanced.data(),h.width,h.height))return 4;
  h.command=wire::Result;
transfer(output,&h,sizeof h,true);
transfer(output,enhanced.data(),enhanced.size(),true);
 }
 }catch(...){return 2;
}}
