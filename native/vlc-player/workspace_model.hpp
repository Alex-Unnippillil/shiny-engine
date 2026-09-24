// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <string_view>
namespace shiny::player {
// All words must occur in the displayed title. Never search hidden paths or URLs.
inline bool matchesTitle(std::wstring_view title, std::wstring_view query) {
    const auto haystack = lower(std::wstring(title));
    const auto needle = lower(std::wstring(query));
    std::size_t start = 0;
    while (start < needle.size()) {
        start = needle.find_first_not_of(L" \t\r\n", start);
        if (start == std::wstring::npos) break;
        const auto end = needle.find_first_of(L" \t\r\n", start);
        if (haystack.find(needle.substr(start, end - start)) == std::wstring::npos) return false;
        if (end == std::wstring::npos) break;
        start = end;
    }
    return true;
}
inline std::vector<std::size_t> queueMatches(const Queue& queue, std::wstring_view query) {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < queue.items.size(); ++i)
        if (matchesTitle(queue.items[i].title, query)) result.push_back(i);
    return result;
}
inline std::optional<std::size_t> mappedQueueIndex(const std::vector<std::size_t>& rows,
                                                  std::ptrdiff_t row, std::size_t size) {
    if (row < 0 || static_cast<std::size_t>(row) >= rows.size()) return {};
    const auto index = rows[static_cast<std::size_t>(row)];
    return index < size ? std::optional<std::size_t>{index} : std::nullopt;
}
}
