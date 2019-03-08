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
#include "consumer.hpp"

// Containers
#include <string>
#include <unordered_set>

// Utility
#include <utility>

// Boost
#include <boost/optional.hpp>

// lock-free queue
#include "readerwriterqueue/readerwriterqueue.h"

// chatload components
#include "format.hpp"


void chatload::consumer::consumeLogs(moodycamel::ReaderWriterQueue<std::wstring>& queue,
                                     std::unordered_set<std::wstring>& out) {
    // Empty string signalizes end of files
    std::wstring file;
    do {
        while (!queue.try_dequeue(file)) {}

        std::wstring::size_type beg, last = 0;
        while ((beg = file.find(L'[', last)) != std::wstring::npos) {
            boost::optional<std::wstring> char_name = chatload::format::extract_name(file, beg);
            if (char_name) { out.insert(std::move(*char_name)); }
            last = beg + 1;
        }
    } while (!file.empty());
}
