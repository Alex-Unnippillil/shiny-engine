// SPDX-License-Identifier: MIT
#include "service.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>
namespace shiny::swapper {
namespace {
struct Handle {
    HANDLE h=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value):h(value) {}
    Handle(Handle&& other) noexcept:h(std::exchange(other.h,INVALID_HANDLE_VALUE)) {}
    Handle(const Handle&)=delete;
    ~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
};
struct Locked {
    std::vector<Handle> handles;
    std::uint64_t bytes=0;
    explicit Locked(const std::filesystem::path& path) {
        if(!localSyntax(path.wstring()))throw std::runtime_error("absolute-local-path-required");
        const auto drive=GetDriveTypeW(path.root_path().c_str());
        if(drive!=DRIVE_FIXED && drive!=DRIVE_REMOVABLE)throw std::runtime_error("local-drive-required");
        auto part=path.root_path();
        auto pin=[&](bool directory){
            Handle h(CreateFileW(part.c_str(),directory?FILE_READ_ATTRIBUTES:GENERIC_READ,
                directory?(FILE_SHARE_READ|FILE_SHARE_WRITE):FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:FILE_FLAG_SEQUENTIAL_SCAN),nullptr));
            if(h.h==INVALID_HANDLE_VALUE)throw std::runtime_error("unreadable-or-locked-path");
            BY_HANDLE_FILE_INFORMATION i{};
            if(GetFileType(h.h)!=FILE_TYPE_DISK || !GetFileInformationByHandle(h.h,&i))throw std::runtime_error("regular-disk-file-required");
            if((i.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || bool(i.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory)
                throw std::runtime_error("reparse-or-wrong-file-kind");
            if(!directory){
                if(i.nNumberOfLinks!=1)throw std::runtime_error("hardlink-rejected");
                bytes=(std::uint64_t(i.nFileSizeHigh)<<32)|i.nFileSizeLow;
                if(!bytes || bytes>MaxBytes)throw std::runtime_error("executable-size-limit");
            }
            handles.push_back(std::move(h));
        };
        pin(true);
        const auto relative=path.relative_path();
        for(auto it=relative.begin();it!=relative.end();){part/=*it;++it;pin(it!=relative.end());}
    }
    HANDLE file()const{return handles.back().h;}
};
struct Crypto {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    Crypto(){
        if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("sha256-unavailable");
        if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;throw std::runtime_error("sha256-unavailable");}
    }
    ~Crypto(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
};
Snapshot fingerprint(const std::filesystem::path& path, Locked& locked, std::stop_token stop){
    Snapshot result{path,{},locked.bytes,recognize(path.filename().wstring()),{}};
    Crypto crypto;
    std::vector<std::uint8_t> buffer(1024*1024);
    std::uint64_t total=0;
    while(total<locked.bytes){
        if(stop.stop_requested())throw std::runtime_error("cancelled");
        DWORD count=0;
        if(!ReadFile(locked.file(),buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr) || !count)throw std::runtime_error("incomplete-read");
        if(!total)result.architecture=executableHeader(std::span(buffer.data(),count),locked.bytes);
        total+=count;
        if(total>locked.bytes || BCryptHashData(crypto.hash,buffer.data(),count,0)<0)throw std::runtime_error("fingerprint-failed");
    }
    std::array<UCHAR,32> digest{};
    if(BCryptFinishHash(crypto.hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)throw std::runtime_error("fingerprint-failed");
    std::ostringstream s;
    for(auto v:digest)s<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(v);
    result.sha256=s.str();return result;
}
}
Snapshot inspect(const std::filesystem::path& path,std::stop_token stop){
    (void)recognize(path.filename().wstring());Locked locked(path);return fingerprint(path,locked,stop);
}
unsigned long launch(const Snapshot& snapshot,bool consent,std::stop_token stop){
    if(!consent)throw std::runtime_error("explicit-launch-consent-required");
    Locked locked(snapshot.path);const auto current=fingerprint(snapshot.path,locked,stop);
    if(current.sha256!=snapshot.sha256 || current.bytes!=snapshot.bytes || current.tool!=snapshot.tool || current.architecture!=snapshot.architecture)throw std::runtime_error("selected-executable-changed-select-again");
    if(stop.stop_requested())throw std::runtime_error("cancelled");
    // Parents and final executable remain locked against replacement until process creation returns.
    // No shell, arguments, inherited handles, automatic elevation, injection or media handoff.
    auto command=L"\""+snapshot.path.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(snapshot.path.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
            snapshot.path.parent_path().c_str(),&startup,&process)){
        if(GetLastError()==ERROR_ELEVATION_REQUIRED)throw std::runtime_error("external-tool-requires-elevation-not-launched");
        throw std::runtime_error("external-process-start-failed");
    }
    const auto pid=process.dwProcessId;CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return pid; // Started is not Ready, Approved, Compatible or DLSS active.
}
}
