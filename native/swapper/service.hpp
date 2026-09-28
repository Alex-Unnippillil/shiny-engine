// SPDX-License-Identifier: MIT
#pragma once
#include "policy.hpp"
#include <filesystem>
#include <stop_token>
namespace shiny::swapper {
struct Snapshot { std::filesystem::path path; std::string sha256; std::uint64_t bytes=0; Tool tool=Tool::dlss5; };
Snapshot inspect(const std::filesystem::path& path, std::stop_token stop = {});
// Consent is per invocation and bound to Snapshot bytes; nothing is persisted.
unsigned long launch(const Snapshot& snapshot, bool consent, std::stop_token stop = {});
}
