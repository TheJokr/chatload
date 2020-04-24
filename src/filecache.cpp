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

// Forward declaration
#include "filecache.hpp"

// C headers
#include <cstdint>

// Streams
#include <fstream>

// Utility
#include <utility>
#include <limits>

// chatload components
#include "common.hpp"


bool chatload::file_cache::save_to_file(const chatload::file_cache::cache_t& cache, const chatload::string& file) {
    std::basic_ofstream<chatload::char_t> out(file, std::basic_ofstream<chatload::char_t>::trunc);
    if (!out) { return false; }

    for (const auto& val : cache) {
        out << val.first << CHATLOAD_STRING('\t') << val.second << CHATLOAD_STRING('\n');
    }

    return true;
}

chatload::file_cache::cache_t chatload::file_cache::load_from_file(const chatload::string& file) {
    chatload::file_cache::cache_t cache;
    std::basic_ifstream<chatload::char_t> in(file);

    chatload::string f;
    std::uint_least64_t wt;  // NOLINT(cppcoreguidelines-init-variables): initialized below
    while (std::getline(in, f, CHATLOAD_STRING('\t'))) {
        in >> wt;
        in.ignore(std::numeric_limits<std::streamsize>::max(), CHATLOAD_STRING('\n'));

        cache.emplace(std::move(f), wt);
    }

    return cache;
}
