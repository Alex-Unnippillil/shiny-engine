// SPDX-License-Identifier: MIT
#include "platform.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <cstdlib>
#endif
namespace shiny::packages {
namespace {
#ifdef _WIN32
struct Handle {
 HANDLE value=INVALID_HANDLE_VALUE;
 explicit Handle(HANDLE h):value(h) {if(h==INVALID_HANDLE_VALUE||!h)throw std::runtime_error("file-unavailable-or-leased");
}
 ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);
}
 Handle(Handle&& other) noexcept:value(std::exchange(other.value,INVALID_HANDLE_VALUE)){}
 Handle(const Handle&)=delete;
Handle& operator=(const Handle&)=delete;
};
void checked(HANDLE handle,bool directory){
 BY_HANDLE_FILE_INFORMATION i{};
 if(GetFileType(handle)!=FILE_TYPE_DISK||!GetFileInformationByHandle(handle,&i))throw std::runtime_error("not-a-local-disk-file");
 if(i.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("reparse-point-rejected");
 if(bool(i.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory)throw std::runtime_error("wrong-file-kind");
 if(!directory&&i.nNumberOfLinks!=1)throw std::runtime_error("hardlink-rejected");
}
#else
struct Handle {
 int value=-1;
 explicit Handle(int fd):value(fd){if(fd<0)throw std::runtime_error("file-unavailable-or-leased");
}
 ~Handle(){if(value>=0)::close(value);
}
 Handle(Handle&& other) noexcept:value(std::exchange(other.value,-1)){}
 Handle(const Handle&)=delete;
Handle& operator=(const Handle&)=delete;
};
void checked(int handle,bool directory){
 struct stat s{};
if(fstat(handle,&s)|| (directory?!S_ISDIR(s.st_mode):!S_ISREG(s.st_mode)))throw std::runtime_error("wrong-file-kind");
 if(!directory&&s.st_nlink!=1)throw std::runtime_error("hardlink-rejected");
}
#endif
}
void cancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("cancelled");
}
bool hashId(std::string_view s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string_view::npos;
}
std::string pathText(const fs::path& p){auto s=p.u8string();
return {reinterpret_cast<const char*>(s.data()),s.size()};
}
fs::path textPath(std::string_view s){return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(s.data()),s.size()));
}
std::string digest(std::span<const std::uint8_t> bytes){
 std::array<unsigned char,32> hash{};
#ifdef _WIN32
 BCRYPT_ALG_HANDLE algorithm=nullptr;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("sha256-unavailable");
 auto status=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),hash.data(),static_cast<ULONG>(hash.size()));
 BCryptCloseAlgorithmProvider(algorithm,0);
if(status<0)throw std::runtime_error("sha256-failed");
#else
 unsigned length=0;
 if(!EVP_Digest(bytes.data(),bytes.size(),hash.data(),&length,EVP_sha256(),nullptr)||length!=32)throw std::runtime_error("sha256-failed");
#endif
 constexpr char hex[]="0123456789abcdef";
std::string out;
out.reserve(64);
 for(auto b:hash){out+=hex[b>>4];
out+=hex[b&15];
}return out;
}
std::string digest(std::string_view s){return digest(std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size()));
}
void validateLocal(const fs::path& p){
 if(!p.is_absolute()||p.empty()||pathText(p).size()>30000)throw std::runtime_error("absolute-local-path-required");
#ifdef _WIN32
 const auto nativePath=p.wstring();
 if(nativePath.size()<3||nativePath[1]!=L':'||!((nativePath[0]>=L'A'&&nativePath[0]<=L'Z')||(nativePath[0]>=L'a'&&nativePath[0]<=L'z'))||nativePath.find(L':',2)!=nativePath.npos||(nativePath[2]!=L'\\'&&nativePath[2]!=L'/'))throw std::runtime_error("absolute-local-drive-path-required");
 if(GetDriveTypeW(p.root_path().c_str())!=DRIVE_FIXED)throw std::runtime_error("fixed-local-drive-required");
#endif
 for(const auto& part:p.relative_path()){
  auto s=pathText(part);
if(s.empty())continue;
  if(s=="."||s==".."||s.back()=='.'||s.back()==' '||s.find_first_of("\r\n\t:*?<>|\"")!=s.npos)throw std::runtime_error("ambiguous-path-rejected");
 }
}
struct DirectoryPin::Impl {std::vector<Handle> handles;
};
DirectoryPin::DirectoryPin(const fs::path& p):impl(std::make_unique<Impl>()){
 validateLocal(p);
auto current=p.root_path();
 auto pin=[&]{
#ifdef _WIN32
  Handle h(CreateFileW(current.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
#else
  Handle h(open(current.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
#endif
  checked(h.value,true);
impl->handles.emplace_back(std::move(h));
 };
pin();
for(auto& part:p.relative_path()){if(part.empty())continue;
current/=part;
pin();
}
}
DirectoryPin::~DirectoryPin()=default;
DirectoryPin::DirectoryPin(DirectoryPin&&) noexcept=default;
DirectoryPin& DirectoryPin::operator=(DirectoryPin&&) noexcept=default;
struct FilePin::Impl {
 DirectoryPin parent;
Handle handle;
 explicit Impl(const fs::path& p):parent(p.parent_path()),handle(
#ifdef _WIN32
 CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr)
#else
 open(p.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)
#endif
 ){checked(handle.value,false);
}
};
FilePin::FilePin(const fs::path& p){validateLocal(p);
impl=std::make_unique<Impl>(p);
}
FilePin::~FilePin()=default;
FilePin::FilePin(FilePin&&) noexcept=default;
FilePin& FilePin::operator=(FilePin&&) noexcept=default;
Bytes FilePin::read(std::size_t limit,std::stop_token stop)const{
 cancelled(stop);
std::uint64_t size=0;
#ifdef _WIN32
 LARGE_INTEGER length{},zero{};
if(!GetFileSizeEx(impl->handle.value,&length)||length.QuadPart<0||!SetFilePointerEx(impl->handle.value,zero,nullptr,FILE_BEGIN))throw std::runtime_error("file-read-failed");
size=static_cast<std::uint64_t>(length.QuadPart);
#else
 struct stat s{};
if(fstat(impl->handle.value,&s)||s.st_size<0||lseek(impl->handle.value,0,SEEK_SET)<0)throw std::runtime_error("file-read-failed");
size=static_cast<std::uint64_t>(s.st_size);
#endif
 if(!size||size>limit)throw std::runtime_error("file-size-limit");
Bytes bytes(static_cast<std::size_t>(size));
std::size_t offset=0;
 while(offset<bytes.size()){
  cancelled(stop);
auto n=std::min<std::size_t>(65536,bytes.size()-offset);
#ifdef _WIN32
  DWORD got=0;
if(!ReadFile(impl->handle.value,bytes.data()+offset,static_cast<DWORD>(n),&got,nullptr)||!got)throw std::runtime_error("file-read-failed");
#else
  auto got=::read(impl->handle.value,bytes.data()+offset,n);
if(got<=0)throw std::runtime_error("file-read-failed");
#endif
  offset+=static_cast<std::size_t>(got);
 }return bytes;
}
#ifdef _WIN32
namespace {
std::wstring mutexName(const fs::path& p){
 // Normalize aliases (including short paths) by stable volume/file identity.
 Handle folder(CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
 checked(folder.value,true);
BY_HANDLE_FILE_INFORMATION info{};
 if(!GetFileInformationByHandle(folder.value,&info))throw std::runtime_error("store-identity-unavailable");
 return L"Local\\ShinyPackages-"+std::to_wstring(info.dwVolumeSerialNumber)+L"-"+std::to_wstring(info.nFileIndexHigh)+L"-"+std::to_wstring(info.nFileIndexLow);
}
}
#endif
struct StoreLock::Impl {
#ifdef _WIN32
 Handle handle;
bool locked=false;
 explicit Impl(const fs::path& p):handle(CreateMutexW(nullptr,FALSE,mutexName(p).c_str())){
  auto r=WaitForSingleObject(handle.value,0);
locked=r==WAIT_OBJECT_0||r==WAIT_ABANDONED;
if(!locked)throw std::runtime_error("store-busy");
 }
 ~Impl(){if(locked)ReleaseMutex(handle.value);
}
#else
 Handle handle;
 explicit Impl(const fs::path& p):handle(open((p/"store.lock").c_str(),O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600)){
  checked(handle.value,false);
if(flock(handle.value,LOCK_EX|LOCK_NB))throw std::runtime_error("store-busy");
 }
#endif
};
StoreLock::StoreLock(const fs::path& p):impl(std::make_unique<Impl>(p)){}StoreLock::~StoreLock()=default;
struct PackageLease::Impl {DirectoryPin parent;
Handle handle;
 Impl(const fs::path& p,bool exclusive):parent(p.parent_path()),handle(
#ifdef _WIN32
 CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,exclusive?0:FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)
#else
 open(p.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600)
#endif
 ) {checked(handle.value,false);
#ifndef _WIN32
 if(flock(handle.value,(exclusive?LOCK_EX:LOCK_SH)|LOCK_NB))throw std::runtime_error("package-leased");
#endif
 }};
PackageLease::PackageLease(const fs::path& p,bool exclusive):impl(std::make_unique<Impl>(p,exclusive)){}PackageLease::~PackageLease()=default;
void makeDirectory(const fs::path& p){validateLocal(p);
DirectoryPin parent(p.parent_path());
std::error_code e;
fs::create_directory(p,e);
if(e)throw std::runtime_error("directory-create-failed");
DirectoryPin check(p);
}
void writeNew(const fs::path& p,std::span<const std::uint8_t> bytes,std::stop_token stop){
 validateLocal(p);
DirectoryPin parent(p.parent_path());
cancelled(stop);
#ifdef _WIN32
 Handle h(CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
#else
 Handle h(open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
#endif
 checked(h.value,false);
std::size_t offset=0;
 while(offset<bytes.size()){
  cancelled(stop);
auto n=std::min<std::size_t>(65536,bytes.size()-offset);
#ifdef _WIN32
  DWORD written=0;
if(!WriteFile(h.value,bytes.data()+offset,static_cast<DWORD>(n),&written,nullptr)||!written)throw std::runtime_error("file-write-failed");
#else
  auto written=::write(h.value,bytes.data()+offset,n);
if(written<=0)throw std::runtime_error("file-write-failed");
#endif
  offset+=static_cast<std::size_t>(written);
 }
#ifdef _WIN32
 if(!FlushFileBuffers(h.value))throw std::runtime_error("file-flush-failed");
#else
 if(fsync(h.value))throw std::runtime_error("file-flush-failed");
#endif
}
void writeNew(const fs::path& p,std::string_view s,std::stop_token stop){writeNew(p,std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size()),stop);
}
void promote(const fs::path& from,const fs::path& to){
 DirectoryPin src(from.parent_path()),dest(to.parent_path());
if(fs::exists(to))throw std::runtime_error("destination-already-exists");
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("package-promote-failed");
#else
 if(::rename(from.c_str(),to.c_str()))throw std::runtime_error("package-promote-failed");
 Handle dir(open(to.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
if(fsync(dir.value))throw std::runtime_error("directory-flush-failed");
#endif
}
void eraseFlat(const fs::path& p){
 // Never recurse, follow a link, or delete a directory with nested content.
 std::vector<fs::path> files;
{DirectoryPin dir(p);
for(auto& e:fs::directory_iterator(p)){
  if(files.size()>=20)throw std::runtime_error("unexpected-package-contents");
FilePin check(e.path());
files.push_back(e.path());
 }
 for(auto& f:files){std::error_code e;
if(!fs::remove(f,e)||e)throw std::runtime_error("package-delete-failed");
}}
 std::error_code e;
if(!fs::remove(p,e)||e)throw std::runtime_error("package-delete-failed");
}
fs::path defaultStore(){
#ifdef _WIN32
 PWSTR value=nullptr;
if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DEFAULT,nullptr,&value)))throw std::runtime_error("local-appdata-unavailable");
fs::path base(value);
CoTaskMemFree(value);
 auto parent=base/L"ShinyPlayer";
makeDirectory(parent);
return parent/L"LibraryStore";
#else
 const char* home=getenv("HOME");
if(!home)throw std::runtime_error("home-unavailable");
return fs::path(home)/".shiny-library-test-store";
#endif
}
fs::path executableDirectory(){
#ifdef _WIN32
 std::array<wchar_t,32768> p{};
auto n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));
if(!n||n>=p.size())throw std::runtime_error("executable-location-unavailable");
return fs::path(p.data()).parent_path();
#else
 std::array<char,32768> p{};
auto n=readlink("/proc/self/exe",p.data(),p.size()-1);
if(n<0)throw std::runtime_error("executable-location-unavailable");
return fs::path(std::string(p.data(),static_cast<std::size_t>(n))).parent_path();
#endif
}
std::string hostArchitecture(){
#if defined(_M_X64) || defined(__x86_64__)
 return "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
 return "arm64";
#else
 return "unsupported";
#endif
}
}
