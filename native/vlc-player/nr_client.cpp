// SPDX-License-Identifier: MIT
#include "nr_client.hpp"
#include <malloc.h>
#include <chrono>
namespace shiny::player {
namespace {
void close(HANDLE& h){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);h=nullptr;}
std::filesystem::path worker(){wchar_t path[32768]{};auto n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)throw std::runtime_error("Cannot locate native NR worker.");return std::filesystem::path(path).parent_path()/L"nr"/L"ShinyNrWorker.exe";}
}
NrProcess::NrProcess(const std::wstring& args){
 HANDLE readIn=nullptr,writeOut=nullptr,error=nullptr;LPPROC_THREAD_ATTRIBUTE_LIST attrs=nullptr;std::vector<unsigned char> memory;bool attrsInitialized=false;
 try{
  auto exe=worker();if(!std::filesystem::is_regular_file(exe))throw std::runtime_error("Native NR worker is missing. Install the complete Windows package.");
  SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
  if(!CreatePipe(&readIn,&input,&security,0)||!CreatePipe(&output,&writeOut,&security,0))throw std::runtime_error("Cannot create private NR pipes.");
  SetHandleInformation(input,HANDLE_FLAG_INHERIT,0);SetHandleInformation(output,HANDLE_FLAG_INHERIT,0);
  error=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);if(error==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create worker diagnostics sink.");
  SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);memory.resize(bytes);attrs=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(memory.data());
  if(!InitializeProcThreadAttributeList(attrs,1,0,&bytes))throw std::runtime_error("Cannot initialize worker handle isolation.");
  attrsInitialized=true;HANDLE inherited[]={readIn,writeOut,error};
  if(!UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr))throw std::runtime_error("Cannot restrict inherited worker handles.");
  STARTUPINFOEXW start{};start.StartupInfo.cb=sizeof start;start.StartupInfo.dwFlags=STARTF_USESTDHANDLES;start.StartupInfo.hStdInput=readIn;start.StartupInfo.hStdOutput=writeOut;start.StartupInfo.hStdError=error;start.lpAttributeList=attrs;
  PROCESS_INFORMATION p{};auto command=quote(exe.wstring())+L" "+args;
  job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof limits))throw std::runtime_error("Cannot create bounded worker job.");
  BOOL created=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,exe.parent_path().c_str(),&start.StartupInfo,&p);
  DeleteProcThreadAttributeList(attrs);attrs=nullptr;attrsInitialized=false;
  if(!created)throw std::runtime_error("Native NR worker could not start.");process=p.hProcess;thread=p.hThread;
  if(!AssignProcessToJobObject(job,process))throw std::runtime_error("Cannot isolate native worker lifetime.");
  ResumeThread(thread);close(thread);close(readIn);close(writeOut);close(error);
 }catch(...){if(attrsInitialized)DeleteProcThreadAttributeList(attrs);if(process)TerminateProcess(process,1);close(thread);close(process);close(job);close(input);close(output);close(readIn);close(writeOut);close(error);throw;}
}
NrProcess::~NrProcess(){cancel();if(process)WaitForSingleObject(process,3000);close(process);close(thread);close(job);close(input);close(output);}
void NrProcess::cancel(){if(job)TerminateJobObject(job,1);}
void NrProcess::send(const void* data,size_t n){auto* p=static_cast<const unsigned char*>(data);while(n){DWORD written=0;if(!WriteFile(input,p,static_cast<DWORD>(std::min<size_t>(n,65536)),&written,nullptr)||!written)throw std::runtime_error("NR worker input closed.");p+=written;n-=written;}}
void NrProcess::receive(void* data,size_t n,std::stop_token stop,unsigned timeout){auto* p=static_cast<unsigned char*>(data);auto end=GetTickCount64()+timeout;
 while(n){if(stop.stop_requested())throw std::runtime_error("Neural preview cancelled.");if(GetTickCount64()>end){cancel();throw std::runtime_error("NR worker timed out; original playback is preserved.");}DWORD available=0;
  if(!PeekNamedPipe(output,nullptr,0,nullptr,&available,nullptr))throw std::runtime_error("NR worker closed its output.");
  if(!available){if(WaitForSingleObject(process,0)==WAIT_OBJECT_0)throw std::runtime_error("NR worker exited without a complete response.");Sleep(5);continue;}
  DWORD read=0;if(!ReadFile(output,p,static_cast<DWORD>(std::min<size_t>({n,available,65536})),&read,nullptr)||!read)throw std::runtime_error("NR output truncated.");p+=read;n-=read;
 }
}
std::string NrProcess::readText(std::stop_token stop,unsigned timeout){std::string text;auto end=GetTickCount64()+timeout;
 for(;;){if(stop.stop_requested()||GetTickCount64()>end){cancel();throw std::runtime_error("Native check cancelled or timed out.");}DWORD available=0;BOOL ok=PeekNamedPipe(output,nullptr,0,nullptr,&available,nullptr);
  if(available){if(text.size()+available>8192){cancel();throw std::runtime_error("Oversized native diagnostic response.");}auto offset=text.size();text.resize(offset+available);DWORD read=0;if(!ReadFile(output,text.data()+offset,available,&read,nullptr))throw std::runtime_error("Native diagnostics truncated.");text.resize(offset+read);}
  else if(!ok||WaitForSingleObject(process,0)==WAIT_OBJECT_0)break;else Sleep(10);
 }WaitForSingleObject(process,1000);return text;
}
bool NrProcess::succeeded()const{DWORD code=1;return GetExitCodeProcess(process,&code)&&code==0;}
void NrSession::status(std::string text,bool ready){std::lock_guard lock(mutex);statusText=std::move(text);isReady=ready;}
NrSession::NrSession(const std::filesystem::path& model,const std::string& researchDigest){thread=std::jthread([this,model,researchDigest](std::stop_token stop){try{
 if(!researchDigest.empty()&&(researchDigest.size()!=64||researchDigest.find_first_not_of("0123456789abcdef")!=std::string::npos))throw std::runtime_error("Invalid local research identity.");
 auto args=researchDigest.empty()?L"--serve "+quote(model.wstring()):L"--serve-research "+quote(model.wstring())+L" "+wide(researchDigest);
 auto child=std::make_shared<NrProcess>(args);{std::lock_guard lock(mutex);process=child;}if(stop.stop_requested()){child->cancel();return;}
 nrwire::Header greeting;child->receive(&greeting,sizeof greeting,stop,120000);nrwire::validateReply(greeting);std::string text(greeting.bytes,'\0');child->receive(text.data(),text.size(),stop);
 if(greeting.command!=nrwire::Ready)throw std::runtime_error(text);status(text,true);
 while(!stop.stop_requested()){std::optional<NrImage> frame;{std::unique_lock lock(mutex);changed.wait(lock,stop,[&]{return pending.has_value();});if(stop.stop_requested())break;frame=std::move(pending);pending.reset();}
  auto started=std::chrono::steady_clock::now();child->send(&frame->header,sizeof frame->header);child->send(frame->original.data(),frame->original.size());nrwire::Header result;child->receive(&result,sizeof result,stop);nrwire::validateReply(result,&frame->header);
  if(result.command==nrwire::Error){std::string errorText(result.bytes,'\0');child->receive(errorText.data(),errorText.size(),stop);throw std::runtime_error(errorText);}
  frame->enhanced.resize(result.bytes);child->receive(frame->enhanced.data(),frame->enhanced.size(),stop);frame->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
  {std::lock_guard lock(mutex);complete=std::move(frame);}
 }
 child->cancel();status("Neural preview stopped.");
 }catch(const std::exception& e){status(e.what());}std::lock_guard lock(mutex);done=true;process.reset();});}
NrSession::~NrSession(){thread.request_stop();changed.notify_all();{std::lock_guard lock(mutex);if(process)process->cancel();}if(thread.joinable())thread.join();}
void NrSession::submit(NrImage frame){std::lock_guard lock(mutex);if(!isReady||done)return;frame.header.sequence=++serial;pending=std::move(frame);changed.notify_one();}
std::optional<NrImage> NrSession::take(){std::lock_guard lock(mutex);auto result=std::move(complete);complete.reset();return result;}
std::string NrSession::status(){std::lock_guard lock(mutex);return statusText;}
bool NrSession::ready(){std::lock_guard lock(mutex);return isReady;}
struct NrSource::Pixels {
 std::mutex mutex;unsigned char* data=nullptr;uint32_t width,height,pitch;std::atomic<uint32_t> frame{0};uint32_t previous=0;uint64_t hash=0;
 Pixels(uint32_t w,uint32_t h):width(w),height(h),pitch((w*4+31)&~31u){const size_t bytes=static_cast<size_t>(pitch)*((h+31)&~31u);data=static_cast<unsigned char*>(_aligned_malloc(bytes,64));if(!data)throw std::bad_alloc();memset(data,0,bytes);}
 ~Pixels(){_aligned_free(data);}
 static void* lock(void* p,void** planes){auto* self=static_cast<Pixels*>(p);self->mutex.lock();*planes=self->data;return nullptr;}
 static void unlock(void* p,void*,void* const*){auto* self=static_cast<Pixels*>(p);uint64_t hash=1469598103934665603ull;for(uint32_t y=0;y<self->height;++y)for(uint32_t x=0;x<self->width*4;++x)if(x%4!=3){hash^=self->data[y*self->pitch+x];hash*=1099511628211ull;}if(hash!=self->hash){self->hash=hash;++self->frame;}self->mutex.unlock();}
 static void display(void*,void*){}
};
NrSource::NrSource(std::shared_ptr<VlcApi> api,const Item& item,uint32_t width,uint32_t height,int64_t at){
 if(!width||!height||width>16384||height>16384)throw std::runtime_error("Invalid preview source dimensions.");
 auto ratio=std::min(1.,512./std::max(width,height));auto w=static_cast<uint32_t>(std::lround(width*ratio)),h=static_cast<uint32_t>(std::lround(height*ratio));
 if(w<33||h<33)throw std::runtime_error("Video aspect ratio is too extreme for the native neural preview.");
 pixels=std::make_unique<Pixels>(w,h);engine=std::make_unique<Engine>(api,nullptr,true,true);api->SetCallbacks(engine->player,Pixels::lock,Pixels::unlock,Pixels::display,pixels.get());api->SetFormat(engine->player,"RV32",w,h,pixels->pitch);engine->open(item);
 (void)at; // Panel seeks when the decoder becomes seekable.
}
NrSource::~NrSource(){engine.reset();pixels.reset();}
std::optional<NrImage> NrSource::sample(){auto n=pixels->frame.load();if(!n||n==pixels->previous)return std::nullopt;pixels->previous=n;
 NrImage result;result.header.width=pixels->width;result.header.height=pixels->height;result.header.bytes=pixels->width*pixels->height*4;result.header.sequence=n;result.original.resize(result.header.bytes);
 std::lock_guard lock(pixels->mutex);for(size_t i=0;i<result.original.size();i+=4){auto j=(i/4/pixels->width)*pixels->pitch+(i/4%pixels->width)*4;result.original[i]=pixels->data[j+2];result.original[i+1]=pixels->data[j+1];result.original[i+2]=pixels->data[j];result.original[i+3]=255;}return result;
}
void NrSource::pause(bool pause){engine->pause(pause);}void NrSource::seek(int64_t t){engine->seek(t);}int64_t NrSource::time()const{return engine->time();}bool NrSource::playing()const{return engine->playing();}bool NrSource::seekable()const{return engine->seekable();}
}
