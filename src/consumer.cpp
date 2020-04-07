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
#include <vector>

// Exceptions
#include <stdexcept>
#include <system_error>

// Utility
#include <functional>
#include <utility>
#include <chrono>

// Boost
#include <boost/system/system_error.hpp>
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/asio/io_context.hpp>

// LZ4 compression
#include <lz4frame.h>

// chatload components
#include "cli.hpp"
#include "format.hpp"
#include "compressor.hpp"
#include "network.hpp"


namespace {
constexpr LZ4F_preferences_t chatload_lz4f_preferences() noexcept {
    LZ4F_preferences_t prefs = {};
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.blockMode = LZ4F_blockLinked;
    prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    return prefs;
}

constexpr boost::u16string_view extract_name_sv(const std::u16string& file, std::u16string::size_type header_beg) {
    // std::u16string::find guarantees that this is valid
    return { file.data() + header_beg, file.length() - header_beg };
}

std::system_error to_std_error(const boost::system::system_error& error) {
    return { error.code(), error.std::runtime_error::what() };
}
}  // Anonymous namespace


chatload::consumer::consume_stat chatload::consumer::consumeLogs(const chatload::cli::options& args, queue_t& queue) {
    chatload::consumer::consume_stat res;
    const auto start_time = std::chrono::system_clock::now();
    bool reader_finished = false;

    try {
        consumer instance(args);
        try {
            instance.run(queue, reader_finished, res);
        } catch (const std::runtime_error&) {
            // Attempt graceful connection shutdown after constructor finished
            instance.ctx.for_each(std::mem_fn(&chatload::network::tcp_writer::shutdown));
            instance.ctx.io_ctx.run();
            throw;
        }

        // Collect any host-specific errors for res
        // res.error is default-initialized to the first type
        std::vector<host_status>& host_stat = boost::variant2::get<0>(res.error);
        host_stat.reserve(instance.ctx.writers.size());
        instance.ctx.for_each([&host_stat](chatload::network::tcp_writer& writer) {
            host_stat.push_back({ writer.get_host(), writer.get_error().map(to_std_error) });
        });
    } catch (const boost::system::system_error& ex) {
        // From chatload::network::clients_context constructor
        res.error = to_std_error(ex);
    } catch (const std::system_error& ex) {
        // From chatload::streaming_optional_lz4_compressor
        res.error = ex;
    }

    if (!reader_finished) {
        // Ignore files until reader is done (empty string)
        std::u16string file;
        do {
            while (!queue.try_dequeue(file)) {}
        } while (!file.empty());
    }

    res.duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_time);
    return res;
}

chatload::consumer::consumer(const chatload::cli::options& args) :
        compressor(chatload_lz4f_preferences(), this->next_comp_buf), ctx(args) {}

void chatload::consumer::run(queue_t& queue, bool& reader_finished, consume_stat& res) {
    // LZ4 compression
    const bool compressor_active = this->compressor.is_compressing();
    const auto push_next_comp_buf = [this](chatload::network::tcp_writer& writer) {
        writer.push_buffer(this->next_comp_buf.get());
    };

    // Establish socket connections
    boost::asio::io_context& io_ctx = this->ctx.io_ctx;
    if (this->next_comp_buf) {
        res.size_compressed += this->next_comp_buf.get().size();
        this->ctx.for_each(push_next_comp_buf);
    }
    io_ctx.run();
    io_ctx.restart();

    std::u16string file;
    chatload::streaming_optional_lz4_compressor::buffer_t file_buf;
    file_buf.reserve(AVG_BUF_SIZE);

    // Extract character names
    for (std::size_t iteration = 0;; ++iteration) {
        while (!queue.try_dequeue(file)) {}
        if (file.empty()) {
            // Empty string signals end of files
            break;
        }

        std::u16string::size_type beg, last = 0;
        while ((beg = file.find('[', last)) != std::u16string::npos) {
            last = beg + 1;
            boost::optional<boost::u16string_view> char_name = chatload::format::extract_name(extract_name_sv(file, beg));
            if (!char_name) { continue; }

            auto& name = char_name.get();
            if (!this->cache.add_if_absent(name)) { continue; }
            ++res.names_processed;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): cast to bytes
            auto name_ptr = reinterpret_cast<const unsigned char*>(name.data());
            const auto name_len = sizeof(boost::u16string_view::value_type) * name.size();

            // Insert UTF-16LE newline (0x0A00) after name
            file_buf.insert(file_buf.cend(), name_ptr, name_ptr + name_len);
            file_buf.push_back('\n');
            file_buf.push_back(0);
        }

        bool run_io_poll = iteration % FILES_PER_IO_POLL == 0;
        if (!file_buf.empty()) {
            this->next_comp_buf = this->compressor.push_chunk(std::move(file_buf));
            file_buf.clear();

            if (this->next_comp_buf) {
                res.size_compressed += this->next_comp_buf.get().size();
                this->ctx.for_each(push_next_comp_buf);
                run_io_poll |= compressor_active;  // always poll for new compressed buffers
            }
        }

        if (run_io_poll) {
            io_ctx.poll();
            io_ctx.restart();

            if (iteration % (FILES_PER_IO_POLL * IO_POLL_PER_ERR_CHECK) == 0 && this->ctx.all_down()) {
                // There won't be any further progress on uploads, abort now
                return;
            }
        }
    }
    reader_finished = true;

    this->next_comp_buf = this->compressor.finalize();
    if (this->next_comp_buf) {
        res.size_compressed += this->next_comp_buf.get().size();
        this->ctx.for_each(push_next_comp_buf);
    }
    this->ctx.for_each(std::mem_fn(&chatload::network::tcp_writer::shutdown));

    // Run until all data is sent and sockets are closed
    io_ctx.run();
}
