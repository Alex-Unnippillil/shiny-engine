// SPDX-License-Identifier: MIT
// Synthetic test-only DLL; never package as a runtime. If executed rather than
// inspected as data, it terminates only the audit test child with code 77.
#include <windows.h>
extern "C" __declspec(dllexport) int audit_fixture_only() { return 0; }
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) ExitProcess(77);
    return TRUE;
}
