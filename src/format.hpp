/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2017  Leo Blöcher
 *
 * This file is part of chatload-client.
 *
 * chatload-client is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * chatload-client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with chatload-client.  If not, see <http://www.gnu.org/licenses/>.
 */

// Header guard
#pragma once
#ifndef CHATLOAD_FORMAT_H
#define CHATLOAD_FORMAT_H


// C headers
#include <cstdlib>
#include <cwchar>

// Streams
#include <sstream>

// Containers
#include <string>

// Utility
#include <utility>
#include <chrono>

namespace chatload {
namespace format {
inline std::pair<long double, std::string> format_size(unsigned long long bytes) {
    long double size = static_cast<long double>(bytes);

    std::string prefix = "gigabyte";
    for (const auto val : { "byte", "kilobyte", "megabyte" }) {
        if (size < 1000) {
            prefix = val;
            break;
        }
        size /= 1000;
    }
    if (size > 1) { prefix += 's'; }

    return { size, std::move(prefix) };
}

template<class Rep, class Period>
inline std::string format_duration(const std::chrono::duration<Rep, Period>& dur) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur).count();

    bool neg = secs < 0;
    if (neg) { secs = -secs; }

    auto hours = secs / 3600;
    secs %= 3600;
    auto mins = secs / 60;
    secs %= 60;

    std::ostringstream oss;
    if (neg) { oss << "-"; }
    if (hours) { oss << hours << "h"; }
    if (mins) { oss << mins << "m"; }
    if (secs || (!hours && !mins)) { oss << secs << "s"; }
    return oss.str();
}

inline std::wstring extract_name(const std::wstring& line, std::wstring::size_type header_beg) {
    // Format: [ YYYY.MM.DD HH:mm:ss ] CHARACTER_NAME > TEXT
    // Some lines may be damaged due to missing synchronization on CCP's part
    // See https://community.eveonline.com/support/policies/naming-policy-en/
    constexpr std::wstring::size_type header_len = 22;

    if (header_beg + header_len >= line.size() || line.at(header_beg + header_len) != L']') {
        // Invalid match
        return std::wstring();
    }

    std::wstring::size_type name_beg = header_beg + header_len + 2;
    std::wstring::size_type name_end = name_beg;

    unsigned char num_space = 0;
    const auto line_len = line.size();
    for (auto idx = name_beg; idx < line_len; ++idx) {
        wchar_t cur = line[idx];
        if (!iswalnum(cur) && cur != L'-' && cur != L'\'') {
            if (cur == L' ' && num_space < 2 && idx + 1 < line_len && line.at(idx + 1) != L'>') {
                ++num_space;
            } else {
                name_end = idx;
                break;
            }
        }
    }

    return line.substr(name_beg, name_end - name_beg);
}
}  // namespace format
}  // namespace chatload


#endif  // CHATLOAD_FORMAT_H
