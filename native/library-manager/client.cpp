// SPDX-License-Identifier: MIT
#include "client.hpp"
#include "worker_identity.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <thread>
namespace shiny::packages {
namespace {
void closeHandle(HANDLE& h){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);
h=nullptr;
}
std::wstring quoted(const std::wstring& s){
 // Command-line quoting is for CreateProcess only, never a shell.
 std::wstring out=L"\"";
unsigned slashes=0;
 for(auto c:s){if(c==L'\\'){++slashes;
continue;
}if(c==L'\"'){out.append(slashes*2+1,L'\\');
out+=c;
}else{out.append(slashes,L'\\');
out+=c;
}slashes=0;
}
 out.append(slashes*2,L'\\');
return out+L'\"';
}
}
struct WorkerClient::Impl {
 std::unique_ptr<FilePin> executablePin;
 HANDLE process=nullptr,job=nullptr,input=nullptr,output=nullptr;
 std::uint32_t backendRevision=0;
std::uint64_t sequence=0;
 void cancel(){if(job)TerminateJobObject(job,1);
}
 void clean(){cancel();
if(process)WaitForSingleObject(process,3000);
closeHandle(process);
closeHandle(job);
closeHandle(input);
closeHandle(output);
}
 std::jthread watchdog(std::stop_token external){
  return std::jthread([this,external](std::stop_token finished){auto deadline=GetTickCount64()+10000;
   while(!finished.stop_requested()){if(external.stop_requested()||GetTickCount64()>=deadline){cancel();return;}Sleep(5);}
  });
 }
 void transfer(void* data,std::size_t bytes,bool write){auto* p=static_cast<std::uint8_t*>(data);
  while(bytes){DWORD n=0;
auto amount=static_cast<DWORD>(std::min<std::size_t>(bytes,65536));
BOOL ok=write?WriteFile(input,p,amount,&n,nullptr):ReadFile(output,p,amount,&n,nullptr);
   if(!ok||!n)throw std::runtime_error("enhancement-worker-closed-cancelled-or-timed-out");
p+=n;
bytes-=n;
  }
 }
 Impl(const fs::path& root,const std::string& id,std::stop_token stop){
  if(!hashId(id))throw std::runtime_error("invalid-package-id");
validateLocal(root);
cancelled(stop);
  auto exe=executableDirectory()/L"ShinyEnhancementWorker.exe";
  executablePin=std::make_unique<FilePin>(exe);
  if(digest(executablePin->read(32*1024*1024,stop))!=compiledWorkerSha256)throw std::runtime_error("worker-integrity-mismatch");
  HANDLE readInput=nullptr,writeOutput=nullptr,error=nullptr,thread=nullptr;
LPPROC_THREAD_ATTRIBUTE_LIST attrs=nullptr;
bool initialized=false;
std::vector<std::uint8_t> storage;
  try{
   SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
   if(!CreatePipe(&readInput,&input,&sa,0)||!CreatePipe(&output,&writeOutput,&sa,0))throw std::runtime_error("private-pipe-create-failed");
   if(!SetHandleInformation(input,HANDLE_FLAG_INHERIT,0)||!SetHandleInformation(output,HANDLE_FLAG_INHERIT,0))throw std::runtime_error("private-pipe-isolation-failed");
   error=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
   if(error==INVALID_HANDLE_VALUE)throw std::runtime_error("worker-error-sink-failed");
   SIZE_T bytes=0;
InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
storage.resize(bytes);
attrs=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
   if(!InitializeProcThreadAttributeList(attrs,1,0,&bytes))throw std::runtime_error("worker-handle-list-failed");
initialized=true;
   HANDLE handles[]={readInput,writeOutput,error};
   if(!UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof handles,nullptr,nullptr))throw std::runtime_error("worker-handle-list-failed");
   STARTUPINFOEXW start{};
start.StartupInfo.cb=sizeof start;
start.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
start.StartupInfo.hStdInput=readInput;
start.StartupInfo.hStdOutput=writeOutput;
start.StartupInfo.hStdError=error;
start.lpAttributeList=attrs;
   job=CreateJobObjectW(nullptr,nullptr);
JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
   limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
limits.BasicLimitInformation.ActiveProcessLimit=1;
   if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof limits))throw std::runtime_error("worker-job-failed");
   auto command=quoted(exe.wstring())+L" --store-dir "+quoted(root.wstring())+L" --package "+textPath(id).wstring();
PROCESS_INFORMATION child{};
   if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,exe.parent_path().c_str(),&start.StartupInfo,&child))throw std::runtime_error("worker-start-failed");
   process=child.hProcess;
thread=child.hThread;
   if(!AssignProcessToJobObject(job,process))throw std::runtime_error("worker-job-assignment-failed");
   if(ResumeThread(thread)==static_cast<DWORD>(-1))throw std::runtime_error("worker-resume-failed");
   DeleteProcThreadAttributeList(attrs);
initialized=false;
closeHandle(thread);
closeHandle(readInput);
closeHandle(writeOutput);
closeHandle(error);
   auto guard=watchdog(stop);
wire::Greeting greeting;
transfer(&greeting,sizeof greeting,false);
cancelled(stop);
   if(greeting.magicValue!=wire::magic||greeting.abiValue!=wire::abi||greeting.format!=1||std::string(greeting.package.begin(),greeting.package.end())!=id||(greeting.revision!=1&&greeting.revision!=2))throw std::runtime_error("worker-handshake-mismatch");
   backendRevision=greeting.revision;
  }catch(...){if(initialized)DeleteProcThreadAttributeList(attrs);
if(process)TerminateProcess(process,1);
closeHandle(thread);
closeHandle(readInput);
closeHandle(writeOutput);
closeHandle(error);
clean();
throw;
}
 }
 ~Impl(){clean();
}
};
WorkerClient::WorkerClient(const fs::path& root,const std::string& id,std::stop_token stop):impl(std::make_unique<Impl>(root,id,stop)){}
WorkerClient::~WorkerClient()=default;
void WorkerClient::cancel(){impl->cancel();
}
std::uint32_t WorkerClient::revision()const{return impl->backendRevision;
}
Bytes WorkerClient::process(wire::FrameHeader request,std::span<const std::uint8_t> original,std::stop_token stop){
 cancelled(stop);
request.sequence=++impl->sequence;
request.command=wire::Frame;
wire::validate(request);
 if(original.size()!=request.bytes)throw std::runtime_error("input-frame-size-mismatch");
auto guard=impl->watchdog(stop);
 impl->transfer(&request,sizeof request,true);
impl->transfer(const_cast<std::uint8_t*>(original.data()),original.size(),true);
 wire::FrameHeader response;
impl->transfer(&response,sizeof response,false);
wire::validate(response,true);
 if(response.command!=wire::Result||response.sequence!=request.sequence||response.generation!=request.generation||response.ptsMs!=request.ptsMs||response.width!=request.width||response.height!=request.height||response.bytes!=request.bytes)throw std::runtime_error("worker-response-mismatch");
 Bytes result(response.bytes);
impl->transfer(result.data(),result.size(),false);
cancelled(stop);
return result;
}
void WorkerClient::reset(std::uint64_t generation,std::stop_token stop){
 cancelled(stop);
wire::FrameHeader request;
request.command=wire::Reset;
request.sequence=++impl->sequence;
request.generation=generation;
 auto guard=impl->watchdog(stop);
impl->transfer(&request,sizeof request,true);
wire::FrameHeader response;
impl->transfer(&response,sizeof response,false);
wire::validate(response,true);
 if(response.command!=wire::Ack||response.sequence!=request.sequence||response.generation!=request.generation)throw std::runtime_error("worker-reset-mismatch");
cancelled(stop);
}
Bytes probeInput(){Bytes bytes;
for(unsigned y=0;y<5;++y)for(unsigned x=0;x<7;++x)for(unsigned c=0;c<4;++c)bytes.push_back(static_cast<std::uint8_t>(c<3?(x*37+y*11+c*23)%256:(x+y)*7));
return bytes;
}
std::string probeDigest(std::uint32_t revision){if(revision==1)return "b086a1d7394bb19d3470c7ff4a8e479019e9a97e3ae1302a7b805e71fc7ec8d8";
if(revision==2)return "64a9704eb22c94be4a41dc56ea42bcc85220086086d3780baca23b2d2e64a2a7";
throw std::runtime_error("unknown-backend-revision");
}
void probePackage(const fs::path& root,const std::string& id,std::stop_token stop){
 WorkerClient worker(root,id,stop);
wire::FrameHeader frame;
frame.width=7;
frame.height=5;
frame.bytes=140;
auto pixels=probeInput();
 auto result=worker.process(frame,pixels,stop);
if(digest(result)!=probeDigest(worker.revision()))throw std::runtime_error("backend-known-frame-check-failed");
worker.reset(1,stop);
}
}
