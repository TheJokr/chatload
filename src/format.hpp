/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2019  Leo Blöcher
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
#include <cstdint>

// Streams
#include <sstream>
#include <iomanip>

// Containers
#include <string>

// Utility
#include <chrono>

namespace chatload {
namespace format {
inline std::string format_size(std::uint_least64_t bytes) {
    auto size = static_cast<long double>(bytes);
    const char* unit = "gigabyte";
    // NOLINTNEXTLINE(readability-qualified-auto): decreases readability
    for (const auto val : { "byte", "kilobyte", "megabyte" }) {
        if (size < 1000) {
            unit = val;
            break;
        }
        size /= 1000;
    }

    // Format using fixed notation, but trim trailing zeros (and dot, if possible)
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size;

    std::string res = oss.str();
    while (res.back() == '0') { res.pop_back(); }
    if (res.back() == '.') { res.pop_back(); }

    // Add unit (plural if size is not rounded to 1.00)
    res.append(1, ' ').append(unit);
    // NOLINTNEXTLINE(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): not magic
    if (size >= 1.005l || size < 0.995l) { res.append(1, 's'); }
    return res;
}

template<class Rep, class Period>
inline std::string format_duration(const std::chrono::duration<Rep, Period>& dur) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur).count();

    bool neg = secs < 0;
    if (neg) { secs = -secs; }

    auto hours = secs / (60 * 60);
    secs %= (60 * 60);
    auto mins = secs / 60;
    secs %= 60;

    std::ostringstream oss;
    if (neg) { oss << '-'; }
    if (hours) { oss << hours << 'h'; }
    if (mins) { oss << mins << 'm'; }
    if (secs || (!hours && !mins)) { oss << secs << 's'; }
    return oss.str();
}
}  // namespace format
}  // namespace chatload


#endif  // CHATLOAD_FORMAT_H
