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
#ifndef CHATLOAD_NETWORK_H
#define CHATLOAD_NETWORK_H


// Containers
#include <vector>

// Utility
#include <utility>
#include <algorithm>

// Boost
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

// OpenSSL
#include <openssl/ssl.h>

// chatload components
#include "constants.hpp"
#include "cli.hpp"
#include "os.hpp"

namespace chatload {
namespace network {
class tcp_writer {
private:
    boost::asio::io_context& io_ctx;
    boost::beast::ssl_stream<boost::asio::ip::tcp::socket> ssl_sock;
    bool write_queued = true;
    bool shutdown_queued = false;

    // Holds asio::const_buffers for all queued chunks
    std::vector<boost::asio::const_buffer> buffers;

public:
    explicit tcp_writer(const chatload::cli::host& host, boost::asio::io_context& io_ctx,
                        boost::asio::ssl::context& ssl_ctx, boost::asio::ip::tcp::resolver& tcp_resolver);

    inline void schedule() {
        if (!this->write_queued) {
            this->write_queued = true;
            boost::asio::post(this->io_ctx, [this]{ this->write_hdlr({}, {}); });
        }
    }

    template<typename T>
    inline void push_buffer(T&& buffer) {
        this->buffers.push_back(std::forward<T>(buffer));
        this->schedule();
    }

    inline void shutdown() {
        this->shutdown_queued = true;
        this->schedule();
    }

private:
    void resolve_hdlr(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);
    void connect_hdlr(const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint& endpoint);
    void ssl_handshake_hdlr(const boost::system::error_code& ec);
    void write_hdlr(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void ssl_shutdown_hdlr(const boost::system::error_code& ec);
};

struct clients_context {
    boost::asio::io_context io_ctx{1};
    boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::method::tls_client};
    boost::asio::ip::tcp::resolver tcp_resolver{io_ctx};
    std::vector<tcp_writer> writers;

    inline explicit clients_context(const std::vector<chatload::cli::host>& hosts) {
        // TODO: allow optional CAfile/CApath config in addition to OS-native cert store
        // TODO: allow optional cipher_list (TLSv1.2)/ciphersuites (TLSv1.3) config
        SSL_CTX* ssl_ctx_native = this->ssl_ctx.native_handle();
        SSL_CTX_set_min_proto_version(ssl_ctx_native, chatload::OPENSSL_MIN_PROTO_VERSION);
        chatload::os::loadTrustedCerts(ssl_ctx_native);

        this->writers.reserve(hosts.size());
        for (auto& host : hosts) {
            this->writers.emplace_back(host, this->io_ctx, this->ssl_ctx, this->tcp_resolver);
        }
    }

    template<typename T>
    inline void for_each(T&& func) {
        std::for_each(this->writers.begin(), this->writers.end(), std::forward<T>(func));
    }
};
}  // namespace network
}  // namespace chatload


#endif  // CHATLOAD_NETWORK_H
