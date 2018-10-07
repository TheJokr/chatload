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
#include "readerwriterqueue/readerwriterqueue.h"

// chatload components
#include "exception.hpp"
#include "cli.hpp"
#include "os.hpp"
#include "filecache.hpp"


// x86 sports a little-endian memory architecture,
// thus we are able to load the file's content into memory bytewise
bool chatload::reader::readUTF16LE(const std::wstring& path, std::wstring& buffer) {
    std::ifstream in(path, std::fstream::binary | std::fstream::ate);
    if (!in) { return false; }

    std::size_t size = in.tellg();
    if (size <= 2 || size % 2 != 0) { return false; }

    // Ignore BOM
    size -= 2;
    in.seekg(2, std::fstream::beg);
    buffer.resize(size / 2);

    // std::wstring is guaranteed to use contiguous memory
    in.read(reinterpret_cast<char*>(&buffer[0]), size);
    return true;
}

chatload::reader::read_stat chatload::reader::readLogs(chatload::cli::options& args, const std::wregex& pattern,
                                                       moodycamel::ReaderWriterQueue<std::wstring>& queue,
                                                       std::function<void(chatload::os::dir_entry&)> file_cb) {
    chatload::reader::read_stat res;
    const auto start_time = std::chrono::system_clock::now();

    std::wstring log_folder = args.log_folder.value_or_eval([] {
        return chatload::os::GetDocumentsFolder() + LR"(\EVE\logs\Chatlogs\)";
    });

    chatload::os::dir_list log_dir;
    try {
        log_dir = chatload::os::dir_list(log_folder);
    } catch (std::runtime_error&) {
        throw chatload::runtime_error(L"Failed to search for logs in " + log_folder);
    }

    chatload::file_cache::cache_t cache;
    if (args.use_cache) {
        cache = chatload::file_cache::load_from_file(args.cache_file);
    }

    while (log_dir.fetch_file()) {
        chatload::os::dir_entry file = log_dir.get_file();
        if (cache[file.name] >= file.write_time || !std::regex_match(file.name, pattern)) { continue; }

        // If readUTF16LE returns true, buf.empty() always returns false
        std::wstring buf;
        if (!chatload::reader::readUTF16LE(log_folder + file.name, buf)) { continue; }
        while (!queue.try_enqueue(std::move(buf))) {}

        cache[file.name] = file.write_time;

        ++res.files_read;
        res.bytes_read += file.size;
        if (file_cb) { file_cb(file); }
    }
    // Empty string signalizes end of files
    queue.enqueue(std::wstring());

    chatload::file_cache::save_to_file(cache, args.cache_file);
    res.duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_time);
    return res;
}