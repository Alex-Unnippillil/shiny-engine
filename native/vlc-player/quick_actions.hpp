// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <vector>
namespace shiny::ui {
struct QuickAction { int id; std::wstring title, detail, shortcut; bool enabled = true; };
// Returns an existing allowlisted UI command, never interprets typed input as code/path.
std::optional<int> quickActions(HWND owner, const std::vector<QuickAction>& actions);
}
