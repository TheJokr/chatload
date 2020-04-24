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
#include "reader.hpp"

// Streams
#include <fstream>

// Containers
#include <string>

// Exceptions
#include <stdexcept>

// Utility
#include <utility>
#include <functional>
#include <chrono>
#include <regex>

// lock-free queue
#include <readerwriterqueue.h>

// chatload components
#include "common.hpp"
#include "cli.hpp"
#include "os.hpp"
#include "filecache.hpp"


// x86 sports a little-endian memory architecture,
// thus we are able to load the file's content into memory bytewise
bool chatload::reader::readUTF16LE(const chatload::string& path, std::u16string& buffer) {
    std::ifstream in(path, std::ifstream::binary | std::ifstream::ate);
    if (!in) { return false; }

    auto size = static_cast<std::size_t>(in.tellg());
    if (size <= 2 || size % 2 != 0) { return false; }

    // Ignore BOM
    size -= 2;
    in.seekg(2, std::ifstream::beg);
    buffer.resize(size / 2);

    // std::basic_string is guaranteed to use contiguous memory
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): cast to bytes
    in.read(reinterpret_cast<char*>(&buffer[0]), size);
    return true;
}

chatload::reader::read_stat chatload::reader::readLogs(const chatload::cli::options& args,
                                                       const std::basic_regex<chatload::char_t>& pattern,
                                                       moodycamel::ReaderWriterQueue<std::u16string>& queue,
                                                       // NOLINTNEXTLINE(performance-unnecessary-value-param)
                                                       std::function<void(const chatload::os::dir_entry&)> file_cb) {
    chatload::reader::read_stat res{};
    const auto start_time = std::chrono::system_clock::now();

    chatload::string log_folder = args.log_folder.value_or_eval(chatload::os::getLogFolder);
    chatload::os::dir_handle log_dir(log_folder);

    auto cache_file = args.cache_file ? args.cache_file : chatload::os::getCacheFile();
    chatload::file_cache::cache_t cache;
    if (args.use_cache && cache_file) { cache = chatload::file_cache::load_from_file(cache_file.get()); }

    log_folder.append(1, chatload::PATH_SEP);
    const auto base_end = log_folder.length();
    for (const chatload::os::dir_entry& file : log_dir) {
        if (cache[file.name] >= file.write_time || !std::regex_match(file.name, pattern)) { continue; }
        log_folder.replace(base_end, chatload::string::npos, file.name);

        // If readUTF16LE returns true, buf.empty() always returns false
        std::u16string buf;
        if (!chatload::reader::readUTF16LE(log_folder, buf)) { continue; }
        while (!queue.try_enqueue(std::move(buf))) {}  // NOLINT(bugprone-use-after-move): not *actually* moved

        cache[file.name] = file.write_time;

        ++res.files_read;
        res.bytes_read += file.size;
        if (file_cb) { file_cb(file); }
    }
    // Empty string signalizes end of files
    queue.enqueue(std::u16string());

    if (cache_file) { chatload::file_cache::save_to_file(cache, cache_file.get()); }
    res.duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_time);
    return res;
}
