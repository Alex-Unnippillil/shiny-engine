// SPDX-License-Identifier: MIT
#pragma once
#include "nr_client.hpp"
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
