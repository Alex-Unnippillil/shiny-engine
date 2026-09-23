// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
namespace shiny::libraries {
// Explicit local paths only. Returns path-redacted JSON. Never loads inspected code.
std::string auditVlc(const std::filesystem::path& directory);
std::string auditCandidate(const std::filesystem::path& file);
}
