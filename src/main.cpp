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

// Containers
#include <string>
#include <vector>

// Exceptions
#include <exception>
#include <stdexcept>
#include <system_error>

// Threading
#include <future>

// Utility
#include <functional>
#include <chrono>
#include <regex>

// Boost
#include <boost/variant2/variant.hpp>
#include <boost/program_options/errors.hpp>

// lock-free queue
#include <readerwriterqueue.h>

// chatload components
#include "common.hpp"
#include "constants.hpp"
#include "cli.hpp"
#include "os.hpp"
#include "reader.hpp"
#include "consumer.hpp"
#include "format.hpp"

// main function name
#ifdef _WIN32
#define CHATLOAD_MAIN_FUNC_NAME wmain  // NOLINT(cppcoreguidelines-macro-usage)
#else  // !_WIN32
#define CHATLOAD_MAIN_FUNC_NAME main  // NOLINT(cppcoreguidelines-macro-usage)
#endif  // _WIN32


namespace {
constexpr std::chrono::seconds ASYNC_WAIT_TICK(1);
constexpr std::size_t MAX_QUEUE_ENTRIES = 30;
using queue_t = moodycamel::ReaderWriterQueue<std::u16string>;

chatload::reader::read_stat run_reader(const chatload::cli::options& args, queue_t& queue) {
    using regex_t = std::basic_regex<chatload::char_t>;
    regex_t filename_regex(args.regex, regex_t::optimize | regex_t::nosubs | regex_t::ECMAScript);

    if (args.verbose) {
        chatload::cout << "Files read:\n";
        return chatload::reader::readLogs(args, filename_regex, queue, [](const chatload::os::dir_entry& file) {
            chatload::cout << file.name << " (" << file.size << " byte" << (file.size != 1 ? "s)\n" : ")\n");
        });
    } else {
        chatload::cout << "Reading files...\n";
        return chatload::reader::readLogs(args, filename_regex, queue);
    }
}

struct consumer_error_visitor {
    const chatload::consumer::consume_stat& consume_res;

    bool operator()(const std::vector<chatload::consumer::host_status>& host_status) const {
        std::size_t err_hosts = 0;
        for (const auto& stat : host_status) {
            if (stat.error) {
                ++err_hosts;
                const std::system_error& ex = stat.error.get();

                chatload::cerr << "ERROR (" << stat.host.name.c_str();
                if (stat.host.port != chatload::DEFAULT_PORT) { chatload::cerr << ':' << stat.host.port.c_str(); }
                chatload::cerr << "): " << ex.what() << '\n';
            }
        }
        if (err_hosts > 0) { chatload::cerr << std::flush; }

        std::string dur = chatload::format::format_duration(this->consume_res.duration);
        if (err_hosts < host_status.size()) {
            std::string bytes_sent = chatload::format::format_size(this->consume_res.size_compressed);
            chatload::cout << "Uploaded " << this->consume_res.names_processed << " character names ("
                           << bytes_sent.c_str() << ") successfully to " << (host_status.size() - err_hosts)
                           << " remote hosts within " << dur.c_str() << std::endl;
        } else {
            chatload::cout << "All " << host_status.size() << " uploads failed within " << dur.c_str() << std::endl;
        }

        return err_hosts > 0;
    }

    bool operator()(const std::system_error& ex) const {
        chatload::cerr << "ERROR: " << ex.what() << std::endl;
        return true;
    }
};


int run_chatload(const chatload::cli::options& args) {
    bool err_res = false;
    chatload::cout << "This app scrapes your EVE Online chat logs for character names and "
                   << "adds them to a configurable set of remote databases\n" << std::endl;

    // Extract character names and upload them asynchronously
    queue_t queue(MAX_QUEUE_ENTRIES);
    auto consumer_fut = std::async(std::launch::async, chatload::consumer::consumeLogs,
                                   std::cref(args), std::ref(queue));

    // Read logs in main thread
    try {
        chatload::reader::read_stat read_res = run_reader(args, queue);

        std::string bytes_read = chatload::format::format_size(read_res.bytes_read);
        std::string dur = chatload::format::format_duration(read_res.duration);
        chatload::cout << "Total of " << read_res.files_read << " files with a size of "
                       << bytes_read.c_str() << " processed within " << dur.c_str() << std::endl;
    } catch (std::system_error& ex) {
        chatload::cerr << "ERROR: " << ex.what() << std::endl;

        // Terminate consumer gracefully and set error exit code
        queue.enqueue(std::u16string());
        err_res = true;
    }

    // Retrieve result from consumer
    chatload::cout << "\nWaiting for uploads to finish..." << std::flush;
    while (consumer_fut.wait_for(ASYNC_WAIT_TICK) == std::future_status::timeout) { chatload::cout << '.'; }
    chatload::cout << " done!" << std::endl;

    chatload::consumer::consume_stat consume_res = consumer_fut.get();
    err_res |= boost::variant2::visit(consumer_error_visitor{ consume_res }, consume_res.error);

    return static_cast<int>(err_res);
}
}  // Anonymous namespace


// NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
int CHATLOAD_MAIN_FUNC_NAME(int argc, chatload::char_t* argv[]) {
    try {
        chatload::cli::options args = chatload::cli::parseArgs(argc, argv);
        return run_chatload(args);
    } catch (const boost::program_options::error& ex) {
        chatload::cerr << "ERROR: " << ex.what() << std::endl;
        chatload::cout << "See -h/--help for allowed options" << std::endl;
    } catch (const std::exception& ex) {
        chatload::cerr << "UNEXPECTED ERROR: " << ex.what() << std::endl;
    }
    return 1;
}
