// SPDX-License-Identifier: MIT
#pragma once
#include "nr_client.hpp"
// Windows GDI+ rendering needs COM declarations even with WIN32_LEAN_AND_MEAN.
#include <objidl.h>
namespace shiny::player {
void openLibraryManager(HWND owner);
class ManagedPanel {
 struct Impl;std::unique_ptr<Impl> impl;
 public:
 ManagedPanel(HWND,std::shared_ptr<VlcApi>,const Item&,uint32_t,uint32_t,int64_t);
 ~ManagedPanel();bool visible()const;
};
int managedPlaybackTest(const std::filesystem::path& runtime,const std::filesystem::path& fixture,const std::filesystem::path& report);
}
