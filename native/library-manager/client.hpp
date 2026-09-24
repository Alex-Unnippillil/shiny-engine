// SPDX-License-Identifier: MIT
#pragma once
#include "store.hpp"
#include "wire.hpp"
namespace shiny::packages {
class WorkerClient {
 struct Impl;std::unique_ptr<Impl> impl;
 public:
 WorkerClient(const fs::path& store,const std::string& id,std::stop_token stop={});
 ~WorkerClient();WorkerClient(const WorkerClient&)=delete;
 void cancel();
 std::uint32_t revision()const;
 Bytes process(wire::FrameHeader request,std::span<const std::uint8_t> original,std::stop_token stop={});
 void reset(std::uint64_t generation,std::stop_token stop={});
};
Bytes probeInput();
std::string probeDigest(std::uint32_t revision);
void probePackage(const fs::path&,const std::string&,std::stop_token stop={});
}
