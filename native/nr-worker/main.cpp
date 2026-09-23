// SPDX-License-Identifier: MIT
// Native MIT OpenDLSS graph, without NVIDIA SDK/runtime/weights. Independent SDR
// frames only; CPU upload/readback costs are explicit, not a zero-copy claim.
#include "model_guard.hpp"
#include "composition.hpp"
#include "nr_graph.h"
#include "reference.h"
#include <fcntl.h>
#include <io.h>
#include <chrono>
#include <iostream>
#include <memory>
using namespace shiny;
namespace {
std::filesystem::path executableDir(){std::vector<wchar_t> p(32768);DWORD n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));if(!n||n>=p.size())throw std::runtime_error("Cannot locate worker.");return std::filesystem::path(std::wstring(p.data(),n)).parent_path();}
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return std::string(s.begin(),s.end());}
void exactRead(void* target,size_t n){auto* p=static_cast<unsigned char*>(target);while(n){int k=_read(_fileno(stdin),p,static_cast<unsigned>(std::min<size_t>(n,65536)));if(k<=0)throw std::runtime_error("NR input closed or truncated.");n-=k;p+=k;}}
void exactWrite(int out,const void* source,size_t n){auto* p=static_cast<const unsigned char*>(source);while(n){int k=_write(out,p,static_cast<unsigned>(std::min<size_t>(n,65536)));if(k<=0)throw std::runtime_error("NR output closed.");n-=k;p+=k;}}
void reply(int fd,uint32_t type,const std::string& message){nrwire::Header h;h.command=type;h.bytes=static_cast<uint32_t>(std::min<size_t>(4096,message.size()));exactWrite(fd,&h,sizeof h);exactWrite(fd,message.data(),h.bytes);}
class Runtime {
 vk::Context context;
 nr::Model model;
 nr::Kernels kernels;
 std::unique_ptr<nr::Graph> graph;
 nr::Activation features;
 vk::Buffer proxy{};
 uint32_t width=0,height=0;
 void releaseFrame(){graph.reset();context.destroyBuffer(features.buffer);context.destroyBuffer(proxy);}
 public:
 Runtime(const std::filesystem::path& modelDir,const std::filesystem::path& program):context(),model(context,utf8(modelDir),true),kernels(context,utf8(program/L"shaders")){
  if(model.blockCount()!=71)throw std::runtime_error("Unexpected block count.");kernels.setSiluTable(ref::siluTable());
 }
 ~Runtime(){releaseFrame();}
 std::vector<uint8_t> process(const nrwire::Header& h,std::span<const uint8_t> rgba){
  nrwire::validate(h);
  if(width!=h.width||height!=h.height){releaseFrame();width=h.width;height=h.height;auto g=nr::Geometry::fromValid(width,height);
   graph=std::make_unique<nr::Graph>(context,model,kernels,g,nr::Graph::Options{});
   features.format=nr::Format::F32;features.rows=g.fullWidth*g.fullHeight;features.channels=16;features.allocRows=nr::alignRows(features.rows);
   features.buffer=context.createBuffer(static_cast<VkDeviceSize>(features.allocRows)*64,false,"shiny NR features");context.fillZero(features.buffer);
   proxy=context.createBuffer(static_cast<VkDeviceSize>(width)*height*16,false,"shiny NR proxy");
  }
  std::vector<float> source(rgba.size());for(size_t i=0;i<rgba.size();++i)source[i]=rgba[i]/255.f;
  context.upload(proxy,source.data(),source.size()*sizeof(float));context.resetDescriptorPool();
  const auto& g=graph->geometry();nr::Kernels::PreprocessArgs p{g.fullWidth,g.fullHeight,width,height,width,height,7,false,h.tone,h.structure,-1.f,0.f};
  auto commands=context.beginCommands();kernels.preprocessFromProxy(commands,proxy,features,p);context.computeBarrier(commands);graph->record(commands,features);context.endAndSubmit(commands,true);
  auto raw=context.download(graph->head().buffer,graph->head().validBytes());std::vector<float> head(raw.size()/4);memcpy(head.data(),raw.data(),raw.size());
  return nrwire::compose(h,rgba,head,g.fullWidth);
 }
 const std::string& device()const{return context.deviceName();}
};
}
int wmain(int argc,wchar_t** argv){
 SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
 int output=_dup(_fileno(stdout));_setmode(output,_O_BINARY);_setmode(_fileno(stdin),_O_BINARY);
 _dup2(_fileno(stderr),_fileno(stdout)); // Upstream printf must not corrupt protocol bytes.
 const bool research=argc==4&&std::wstring(argv[1])==L"--serve-research";
 const bool serve=research||(argc==3&&std::wstring(argv[1])==L"--serve");
 try{
  const auto program=executableDir();_wputenv_s(L"DLSS5VK_PTX_DIR",(program/L"ptx").c_str());
  if(argc==2&&std::wstring(argv[1])==L"--probe"){
   vk::Context context;std::string result="Native Vulkan feature/device creation passed on "+context.deviceName()+". Model approval and inference are separate and not tested by this probe.";exactWrite(output,result.data(),result.size());_close(output);return 0;
  }
  if(argc==3&&std::wstring(argv[1])==L"--inspect"){
   try { nrpolicy::ModelGuard inspected(argv[2],nrpolicy::ModelUse::Inspect);
   std::string result="FILES_VALID sha="+inspected.digest+"\n"+std::to_string(inspected.layout.stages.size())+" stages / "+std::to_string(inspected.layout.tensors)+" tensors verified.\nNo trained inference or quality certification. Local research requires explicit consent for this model.";
   exactWrite(output,result.data(),result.size());_close(output);return 0;
   }catch(const std::exception& e){std::string error="MODEL_INVALID: "+std::string(e.what());exactWrite(output,error.data(),error.size());_close(output);return 3;}
  }
  if(!serve)throw std::runtime_error("Usage: ShinyNrWorker --probe | --inspect MODEL_DIRECTORY | --serve MODEL_DIRECTORY | --serve-research MODEL_DIRECTORY MANIFEST_SHA256");
  std::wstring consent=research?argv[3]:L"";
  nrpolicy::ModelGuard guard(argv[2],research?nrpolicy::ModelUse::LocalResearch:nrpolicy::ModelUse::Reviewed,std::string(consent.begin(),consent.end()));
  Runtime runtime(argv[2],program);reply(output,nrwire::Ready,std::string(research?"LOCAL RESEARCH / UNVERIFIED. ":"Reviewed model. ")+"Prepared "+runtime.device()+"; independent SDR preview, no temporal history.");
  uint32_t lastSequence=0;
  for(;;){nrwire::Header h;exactRead(&h,sizeof h);nrwire::validate(h);if(h.command==nrwire::Stop)break;
   if(!h.sequence||h.sequence<=lastSequence)throw std::runtime_error("Stale or duplicate frame request.");lastSequence=h.sequence;
   std::vector<uint8_t> pixels(h.bytes);exactRead(pixels.data(),pixels.size());auto processed=runtime.process(h,pixels);
   auto response=h;response.command=nrwire::Result;exactWrite(output,&response,sizeof response);exactWrite(output,processed.data(),processed.size());
  }
  _close(output);return 0;
 }catch(const std::exception& e){try{if(serve)reply(output,nrwire::Error,e.what());else exactWrite(output,e.what(),strlen(e.what()));}catch(...){} _close(output);return 2;}
}
