// SPDX-License-Identifier: MIT
#include "audit.hpp"
#include "catalog.hpp"
#include <iostream>
#include <new>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--help") {
        std::cout << "Shiny Library Audit (Windows x64)\n"
            "  ShinyLibraryAudit --vlc-dir <absolute local VLC directory>\n"
            "  ShinyLibraryAudit --candidate <absolute local recognized DLL or VLC executable>\n"
            "Outputs path-redacted JSON to stdout. Redirect it to save a report.\n"
            "No downloads, DLL loading, installation writes, swapping, or activation.\n"
            "Exit 0: audit completed (not approval). Exit 2: invalid request or audit failure.\n";
        return 0;
    }
    try {
        if (argc != 3) throw std::runtime_error("usage-see-help");
        const std::wstring_view mode(argv[1]);
        if (mode == L"--vlc-dir") std::cout << shiny::libraries::auditVlc(argv[2]);
        else if (mode == L"--candidate") std::cout << shiny::libraries::auditCandidate(argv[2]);
        else throw std::runtime_error("unsupported-command-see-help");
        return std::cout ? 0 : 2;
    } catch (const std::filesystem::filesystem_error&) {
        std::cerr << "{\"error\":\"filesystem-error\"}\n";
    } catch (const std::runtime_error& error) {
        std::cerr << "{\"error\":" << shiny::libraries::jsonString(error.what()) << "}\n";
    } catch (...) {
        std::cerr << "{\"error\":\"audit-failed\"}\n";
    }
    return 2;
}
