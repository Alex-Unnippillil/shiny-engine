// SPDX-License-Identifier: MIT
// Test-only program. No third-party swapper, vendor library or media is executed.
#include <windows.h>
int wmain(int argc,wchar_t**){
    if(argc!=1)return 3; // The bridge must not pass media, targets or commands.
    auto file=CreateFileW(L"started.txt",GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 4;
    DWORD bytes=0;const char proof[]="no-arguments";
    const auto ok=WriteFile(file,proof,sizeof(proof)-1,&bytes,nullptr);
    CloseHandle(file);return ok&&bytes==sizeof(proof)-1?0:5;
}
