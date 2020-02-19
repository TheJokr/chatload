/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2020  Leo Blöcher
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
#ifndef CHATLOAD_LZ4_COMPRESSOR_H
#define CHATLOAD_LZ4_COMPRESSOR_H


// Containers
#include <vector>

// Utility
#include <memory>
#include <utility>

// Boost
#include <boost/optional.hpp>
#include <boost/asio/buffer.hpp>

// LZ4 compression
#include <lz4frame.h>

// chatload components
#include "common.hpp"
#include "exception.hpp"

namespace chatload {
class streaming_optional_lz4_compressor {
public:
    using buffer_t = std::vector<unsigned char>;

private:
    std::vector<buffer_t> buf_store;
    std::unique_ptr<LZ4F_cctx, decltype(&LZ4F_freeCompressionContext)> ctx{ nullptr, &LZ4F_freeCompressionContext };
    std::unique_ptr<LZ4F_preferences_t> prefs;

    using optional_asio_buffer = boost::optional<boost::asio::const_buffer>;

    // Reserve capacity for one full-size block + frame end. Since LZ4F_compressBound
    // assumes a worst-case scenario (i.e., blockSize - 1 bytes buffered), adding
    // just one byte would yield such a buffer size. (see constructor)
    const std::size_t max_buffer;

    // buf_store always contains a new (empty) buffer when compressing
    boost::asio::const_buffer flush_comp_buffer(std::size_t comp_written) {
        boost::asio::const_buffer res{ this->buf_store.back().data(), comp_written };
        this->buf_store.emplace_back();
        this->buf_store.back().reserve(this->max_buffer);
        return res;
    }

    boost::asio::const_buffer flush_regular_buffer() {
        auto& buf = this->buf_store.back();
        return { buf.data(), buf.size() };
    }

public:
    // Caller is responsible for passing in an empty boost::optional
    explicit streaming_optional_lz4_compressor(LZ4F_preferences_t prefs, optional_asio_buffer& header_out) :
            max_buffer(LZ4F_compressBound(1, &prefs)) {
        {
            LZ4F_cctx* temp;
            if (LZ4F_createCompressionContext(&temp, LZ4F_VERSION) != 0) { return; }
            this->ctx.reset(temp);
        }

        // Most of the time there will only be the header and one block
        this->buf_store.reserve(2);

        this->buf_store.emplace_back(LZ4F_HEADER_SIZE_MAX);
        std::size_t comp_written = LZ4F_compressBegin(this->ctx.get(), this->buf_store.back().data(),
                                                      LZ4F_HEADER_SIZE_MAX, &prefs);
        if (LZ4F_isError(comp_written)) {
            this->ctx.reset();
            this->buf_store.clear();
            return;
        }
        header_out = this->flush_comp_buffer(comp_written);

        this->prefs = std::make_unique<LZ4F_preferences_t>(prefs);
    }

    optional_asio_buffer push_chunk(buffer_t&& chunk) {
        if (!this->ctx) {
            this->buf_store.push_back(std::move(chunk));
            return this->flush_regular_buffer();
        }

        buffer_t& buf = this->buf_store.back();
        std::size_t buf_cap = LZ4F_compressBound(chunk.size(), this->prefs.get());
        buf.resize(buf_cap);

        std::size_t comp_written = LZ4F_compressUpdate(this->ctx.get(), buf.data(), buf_cap,
                                                       chunk.data(), chunk.size(), nullptr);
        if (LZ4F_isError(comp_written)) {
            throw chatload::runtime_error(chatload::string(CHATLOAD_STRING("LZ4 error: "))
                                          + LZ4F_getErrorName(comp_written));
        } else if (comp_written > 0) {
            return this->flush_comp_buffer(comp_written);
        } else {
            return boost::none;
        }
    }

    optional_asio_buffer finalize() {
        if (!this->ctx) { return boost::none; }

        buffer_t& buf = this->buf_store.back();
        std::size_t buf_cap = LZ4F_compressBound(0, this->prefs.get());
        buf.resize(buf_cap);

        std::size_t comp_written = LZ4F_compressEnd(this->ctx.get(), buf.data(), buf_cap, nullptr);
        if (LZ4F_isError(comp_written)) {
            throw chatload::runtime_error(chatload::string(CHATLOAD_STRING("LZ4 error: "))
                                          + LZ4F_getErrorName(comp_written));
        } else if (comp_written > 0) {
            return this->flush_comp_buffer(comp_written);
        } else {
            return boost::none;
        }
    }
};
}  // namespace chatload


#endif // CHATLOAD_LZ4_COMPRESSOR_H
