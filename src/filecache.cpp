/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2018  Leo Blöcher
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

// Containers
#include <string>

// Utility
#include <utility>
#include <limits>


bool chatload::file_cache::save_to_file(const chatload::file_cache::cache_t& cache, const std::wstring& file) {
    std::wofstream out(file, std::fstream::trunc);
    if (!out) { return false; }

    for (const auto& val : cache) {
        out << val.first << L'\t' << val.second << L'\n';
    }

    return true;
}

chatload::file_cache::cache_t chatload::file_cache::load_from_file(const std::wstring& file) {
    chatload::file_cache::cache_t cache;
    std::wifstream in(file);

    std::wstring f;
    std::uint_least64_t wt;
    while (std::getline(in, f, L'\t')) {
        in >> wt;
        in.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

        cache.emplace(std::move(f), wt);
    }

    return cache;
}
