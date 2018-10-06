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
#include "chatload.hpp"

// Streams
#include <iostream>

// Containers
#include <string>
#include <vector>
#include <unordered_set>

// Exceptions
#include <exception>
#include <stdexcept>

// Threading
#include <thread>

// Utility
#include <iomanip>
#include <regex>

// C++ REST SDK (HTTP Client, JSON)
#include <cpprest/http_client.h>
#include <cpprest/json.h>

// lock-free queue
#include "readerwriterqueue/readerwriterqueue.h"

// chatload components
#include "constants.hpp"
#include "exception.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "os.hpp"
#include "reader.hpp"
#include "consumer.hpp"
#include "network.hpp"
#include "format.hpp"


int main(int, char**) {
    chatload::os::SetTerminalTitle(chatload::NAME);
    chatload::cli::options args;
    try {
        chatload::os::wargs wargs;
        args = chatload::cli::parseArgs(wargs.size(), wargs.data());
    } catch (chatload::runtime_error& ex) {
        // Version/Help
        std::wcout << ex.what_wide() << std::endl;
        return 0;
    } catch (std::runtime_error& ex) {
        // wargs error
        std::wcerr << "ERROR: " << ex.what() << std::endl;
        return 1;
    } catch (std::logic_error& ex) {
        // boost::program_options error
        std::wcerr << "ERROR: " << ex.what() << std::endl;
        std::wcout << "See -h/--help for allowed options" << std::endl;
        return 1;
    }

    chatload::config cfg(L"config.json");  // TODO: remove

    std::wcout << "This app scrapes your EVE Online chat logs for character names and adds them to a database\n"
               << std::endl;


    // Read logs
    std::unordered_set<std::wstring> names;
    moodycamel::ReaderWriterQueue<std::wstring> queue;

    chatload::reader::read_stat res;
    const std::wregex regex(args.regex, std::wregex::optimize | std::wregex::ECMAScript);
    const auto file_cb = [](chatload::os::dir_entry& file) {
        std::wcout << file.name << " (" << file.size << " byte" << (file.size != 1 ? "s)" : ")") << "\n";
    };

    // Extract character names asynchronously
    std::thread consumer(chatload::consumer::consumeLogs, std::ref(queue), std::ref(names));

    try {
        if (args.verbose) {
            std::wcout << "Files read:\n";
            res = chatload::reader::readLogs(args, regex, queue, file_cb);
        } else {
            std::wcout << "Reading files...\n";
            res = chatload::reader::readLogs(args, regex, queue);
        }
    } catch (chatload::runtime_error& ex) {
        std::wcerr << "ERROR: " << ex.what_wide() << std::endl;
        std::wcout << "Failed to read logs, shutting down" << std::endl;

        // Terminate consumer gracefully
        queue.enqueue(std::wstring());
        consumer.join();
        return 1;
    }

    auto fmt = chatload::format::format_size(res.bytes_read);
    std::string dur = chatload::format::format_duration(res.duration);

    std::wcout << "Total of " << res.files_read << " files with a size of " << std::fixed << std::setprecision(2)
               << fmt.first << " " << fmt.second.c_str() << " read within " << dur.c_str() << std::endl;

    consumer.join();
    if (names.empty()) { return 0; }


    // Upload character names
    std::wcout << "\nEstablishing connection(s)...";

    std::vector<pplx::task<void>> reqs;
    try {
        reqs = chatload::network::sendNames(cfg.get(L"POST").as_array(), names);
    } catch (std::exception& ex) {
        std::wcout << "\n";
        std::wcerr << "ERROR: " << ex.what() << std::endl;
        std::wcout << "Failed to read endpoints, shutting down" << std::endl;
        return 1;
    }

    std::wcout << " " << reqs.size() << " connection(s) established\n"
               << "Adding character names to the endpoint(s)" << std::endl;

    try {
        pplx::when_all(reqs.cbegin(), reqs.cend()).wait();
    } catch (web::http::http_exception& ex) {
        std::wcerr << "ERROR: " << ex.what() << std::endl;
        std::wcout << "Failed to add character names to one of the endpoints" << std::endl;
    }

    return 0;
}
