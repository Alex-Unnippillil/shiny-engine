// SPDX-License-Identifier: MIT
#pragma once
// Desktop UI entry only: no CLI, browser/native-message or DLL-loading endpoint.
#ifdef _WIN32
namespace shiny::swapper { void open(void* owner); bool translate(void* nativeMessage); }
#endif
