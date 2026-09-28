// SPDX-License-Identifier: MIT
#include "service.hpp"
#include "bridge.hpp"
#include <windows.h>
#include <commctrl.h>
#include <winioctl.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace shiny::swapper;
namespace fs=std::filesystem;
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,message);}
void junction(const fs::path& from,const fs::path& to){
    fs::create_directory(from);
    const auto substitute=L"\\??\\"+to.wstring();const auto printable=to.wstring();
    struct Mount { DWORD tag; WORD length,reserved; WORD subOffset,subLength,printOffset,printLength; wchar_t paths[4096]; } data{};
    data.tag=IO_REPARSE_TAG_MOUNT_POINT;
    data.subLength=static_cast<WORD>(substitute.size()*sizeof(wchar_t));
    data.printOffset=static_cast<WORD>(data.subLength+sizeof(wchar_t));
    data.printLength=static_cast<WORD>(printable.size()*sizeof(wchar_t));
    data.length=static_cast<WORD>(8+data.printOffset+data.printLength+sizeof(wchar_t));
    check(data.length<sizeof(data.paths),"junction fixture bounds");
    std::memcpy(data.paths,substitute.c_str(),data.subLength+sizeof(wchar_t));
    std::memcpy(reinterpret_cast<char*>(data.paths)+data.printOffset,printable.c_str(),data.printLength+sizeof(wchar_t));
    auto h=CreateFileW(from.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    check(h!=INVALID_HANDLE_VALUE,"junction handle");DWORD returned=0;
    const auto ok=DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&data,static_cast<DWORD>(8+data.length),nullptr,0,&returned,nullptr);
    CloseHandle(h);check(ok!=FALSE,"junction creation");
}
struct Directory{fs::path path;~Directory(){std::error_code e;fs::remove_all(path,e);}};
int wmain(int argc,wchar_t** argv){try{
    check(argc==2,"fixture required");
    Directory dir{fs::temp_directory_path()/(L"shiny-swapper test é "+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()))};
    fs::create_directory(dir.path);
    auto exe=dir.path/L"DLSS 5 Swapper.exe";
    fs::copy_file(argv[1],exe);
    const auto snapshot=inspect(exe);
    check(snapshot.sha256.size()==64&&snapshot.bytes==fs::file_size(exe),"fingerprint");
    check(!fs::exists(dir.path/L"started.txt"),"inspect must not execute");
    rejects([&]{launch(snapshot,false);},"no consent no execution");
    std::stop_source stop;stop.request_stop();
    rejects([&]{inspect(exe,stop.get_token());},"inspection cancellation");
    rejects([&]{launch(snapshot,true,stop.get_token());},"launch cancellation");
    auto wrong=snapshot;wrong.sha256[0]=wrong.sha256[0]=='0'?'1':'0';
    rejects([&]{launch(wrong,true);},"wrong hash");
    const auto original=fs::file_size(exe);
    {std::ofstream f(exe,std::ios::binary|std::ios::app);f.put('x');}
    rejects([&]{launch(snapshot,true);},"changed file");
    fs::resize_file(exe,original);
    fs::create_hard_link(exe,dir.path/L"alias.exe");
    rejects([&]{inspect(exe);},"hardlink rejected");fs::remove(dir.path/L"alias.exe");
    auto h=CreateFileW(exe.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    check(h!=INVALID_HANDLE_VALUE,"test write lock");
    rejects([&]{inspect(exe);},"write lock rejected");CloseHandle(h);
    auto redirect=dir.path/L"redirect";junction(redirect,dir.path);
    rejects([&]{inspect(redirect/exe.filename());},"ancestor reparse rejected");
    fs::remove(redirect);
    auto setup=dir.path/L"DLSS5-Swapper-Setup-2.2.7.exe";fs::copy_file(exe,setup);
    rejects([&]{inspect(setup);},"setup rejected");
    auto dirExe=dir.path/L"DLSS5-Swapper.exe";fs::create_directory(dirExe);
    rejects([&]{inspect(dirExe);},"directory rejected");fs::remove(dirExe);
    {std::ofstream f(dirExe,std::ios::binary);f<<"not a PE";}
    rejects([&]{inspect(dirExe);},"malformed executable rejected");fs::remove(dirExe);
    check(!fs::exists(dir.path/L"started.txt"),"all denials must leave no execution");
    const auto pid=launch(snapshot,true);check(pid!=0,"explicit fixture launch");
    for(int n=0;n<100&&!fs::exists(dir.path/L"started.txt");++n)Sleep(20);
    // Reading after the child closes its exclusive output handle.
    std::string proof;
    for(int n=0;n<100;++n){std::ifstream f(dir.path/L"started.txt");std::getline(f,proof);if(proof=="no-arguments")break;Sleep(10);}
    check(proof=="no-arguments","working directory and no arguments");
    check(inspect(exe).sha256==snapshot.sha256,"launch does not rewrite selected tool");
    // Modeless optional UI: off by default, no media/runtime prerequisite, no implicit launch.
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_STANDARD_CLASSES};InitCommonControlsEx(&cc);
    auto owner=CreateWindowW(L"STATIC",L"Test owner",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(owner!=nullptr,"owner created");
    open(owner);auto panel=FindWindowW(L"ShinyOptionalSwapper",nullptr);check(panel!=nullptr,"optional panel opens");
    check(!IsWindowEnabled(GetDlgItem(panel,306)),"launch starts disabled");
    check(SendMessageW(GetDlgItem(panel,303),BM_GETCHECK,0,0)==BST_UNCHECKED,"consent starts unchecked");
    SendMessageW(GetDlgItem(panel,303),BM_SETCHECK,BST_CHECKED,0);SendMessageW(panel,WM_COMMAND,303,0);
    check(!IsWindowEnabled(GetDlgItem(panel,306)),"consent alone insufficient");
    open(owner);check(FindWindowW(L"ShinyOptionalSwapper",nullptr)==panel,"one panel per owner");
    SendMessageW(panel,WM_COMMAND,305,0);
    check(SendMessageW(GetDlgItem(panel,303),BM_GETCHECK,0,0)==BST_UNCHECKED,"forget clears consent");
    MSG escape{};escape.hwnd=GetDlgItem(panel,304);escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;
    check(translate(&escape),"escape handled");check(!IsWindow(panel),"escape closes panel");
    DestroyWindow(owner);
    std::cout<<checks<<" Windows swapper consent/fingerprint/launch/UI assertions passed; only a synthetic fixture was launched\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
