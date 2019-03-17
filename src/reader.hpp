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
#ifndef CHATLOAD_READER_H
#define CHATLOAD_READER_H


// C headers
#include <cstdint>

// Containers
#include <string>

// Utility
#include <functional>
#include <chrono>
#include <regex>

// lock-free queue
#include "readerwriterqueue/readerwriterqueue.h"

// chatload components
#include "common.hpp"
#include "cli.hpp"
#include "os.hpp"

namespace chatload {
namespace reader {
bool readUTF16LE(const chatload::string& path, std::u16string& buffer);

struct read_stat {
    std::uint_least64_t files_read = 0;
    std::uint_least64_t bytes_read = 0;
    std::chrono::seconds duration;
};

read_stat readLogs(chatload::cli::options& args, const std::basic_regex<chatload::char_t>& pattern,
                   moodycamel::ReaderWriterQueue<std::u16string>& queue,
                   std::function<void(const chatload::os::dir_entry&)> file_cb = {});
}  // namespace reader
}  // namespace chatload


#endif  // CHATLOAD_READER_H
