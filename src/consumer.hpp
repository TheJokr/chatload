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
#ifndef CHATLOAD_CONSUMER_H
#define CHATLOAD_CONSUMER_H


// C headers
#include <cstdint>

// Containers
#include <string>
#include <vector>

// Exceptions
#include <system_error>

// Utility
#include <chrono>

// Boost
#include <boost/variant2/variant.hpp>
#include <boost/optional.hpp>
#include <boost/asio/buffer.hpp>

// lock-free queue
#include <readerwriterqueue.h>

// chatload components
#include "cli.hpp"
#include "stringcache.hpp"
#include "compressor.hpp"
#include "network.hpp"

namespace chatload {
class consumer {
private:
    using queue_t = moodycamel::ReaderWriterQueue<std::u16string>;

public:
    struct host_status {
        const chatload::cli::host& host;
        boost::optional<std::system_error> error;
    };

    struct consume_stat {
        using error_variant = boost::variant2::variant<
                std::vector<host_status>,  // Must be first for default initialization
                std::system_error
        >;

        std::uint_least64_t names_processed = 0;
        std::uint_least64_t size_compressed = 0;
        std::chrono::seconds duration;
        error_variant error;
    };

    static consume_stat consumeLogs(const chatload::cli::options& args, queue_t& queue);

private:
    // Deduplication cache (18 bits index, default 32 bits values -> 1 MiB cache)
    static constexpr std::size_t CACHE_INDEX_BITS = 18;
    chatload::string_cache<CACHE_INDEX_BITS> cache;

    // LZ4 compression
    boost::optional<boost::asio::const_buffer> next_comp_buf;
    chatload::streaming_optional_lz4_compressor compressor;

    // Socket connections
    chatload::network::clients_context ctx;

    // Average name length is ~12 characters (+1 for newline), roughly 20 names per file
    static constexpr std::size_t AVG_BUF_SIZE = 20 * ((12 + 1) * sizeof(std::u16string::value_type));

    // Batch process 10 files before polling the IO loop
    static constexpr std::size_t FILES_PER_IO_POLL = 10;

    // Check for errors on all writers every 5th IO loop poll
    static constexpr std::size_t IO_POLL_PER_ERR_CHECK = 5;

    explicit consumer(const chatload::cli::options& args);
    void run(queue_t& queue, bool& reader_finished, consume_stat& res);
};
}  // namespace chatload


#endif  // CHATLOAD_CONSUMER_H
