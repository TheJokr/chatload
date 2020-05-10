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

// Exceptions
#include <system_error>

// Utility
#include <utility>
#include <functional>
#include <algorithm>

// Boost
#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

// OpenSSL
#include <openssl/err.h>
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
    boost::asio::deadline_timer timeout_timer;
    boost::beast::ssl_stream<boost::asio::ip::tcp::socket> ssl_sock;

    const chatload::cli::host& host;
    boost::optional<std::system_error> net_err;

    // Holds asio::const_buffers for all queued chunks (non-owning!)
    std::vector<boost::asio::const_buffer> buffers;

    // Holds commands read from the server. Currently, anything
    // received after the protocol version exchange is ignored.
    chatload::protocol::command server_buf = chatload::protocol::command::NONE;

    bool write_queued = true;
    enum : unsigned char { SHUTDOWN_READY, SHUTDOWN_REQUESTED, SHUTDOWN_IN_PROGRESS } shutdown_status = SHUTDOWN_READY;

public:
    explicit tcp_writer(const chatload::cli::host& host, boost::asio::io_context& io_ctx,
                        const boost::posix_time::time_duration& timeout, boost::asio::ssl::context& ssl_ctx,
                        boost::asio::ip::tcp::resolver& tcp_resolver);

    const auto& get_host() const noexcept { return this->host; }
    bool has_error() const noexcept { return this->net_err.has_value(); }
    auto retrieve_error() {
        boost::optional<std::system_error> res = std::move(this->net_err);
        this->net_err.reset();
        return res;
    }

    // MUST be called after the first invocation of io_ctx.run() finished.
    // This restarts the bits of tcp_writer that would make that first call block forever.
    void start_after_init();

    void schedule() {
        if (this->write_queued || this->net_err) { return; }
        this->write_queued = true;
        boost::asio::post(this->io_ctx, [this] { this->write_hdlr({}, {}); });
    }

    template<typename T>
    void push_buffer(T&& buffer) {
        if (this->net_err) { return; }
        this->buffers.push_back(std::forward<T>(buffer));
        this->schedule();
    }

    void shutdown() {
        if (this->shutdown_status != SHUTDOWN_READY || this->net_err) { return; }
        this->shutdown_status = SHUTDOWN_REQUESTED;
        this->schedule();
    }

private:
    template<typename... Args>
    void abort_err(Args&&... args) {
        boost::system::error_code ec;
        this->timeout_timer.cancel(ec);  // prevent cancel from throwing
        this->net_err.emplace(std::forward<Args>(args)...);
    }

    void resolve_hdlr(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);
    void connect_hdlr(const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint&);
    void ssl_handshake_hdlr(const boost::system::error_code& ec);
    void version_exchange_hdlr(const boost::system::error_code& ec, std::size_t);
    void read_hdlr(const boost::system::error_code& ec, std::size_t);
    void write_hdlr(const boost::system::error_code& ec, std::size_t);
    void ssl_shutdown_hdlr(const boost::system::error_code& ec);
    void timeout_hdlr(const boost::system::error_code& ec);

    void shutdown_immediate();
};

struct clients_context {
    boost::asio::io_context io_ctx{ 1 };
    boost::asio::ssl::context ssl_ctx{ boost::asio::ssl::context::method::tls_client };
    boost::asio::ip::tcp::resolver tcp_resolver{ io_ctx };
    std::vector<tcp_writer> writers;

    explicit clients_context(const chatload::cli::options& args) {
        this->ssl_ctx.set_verify_mode(args.insecure_tls ? boost::asio::ssl::verify_none : boost::asio::ssl::verify_peer);
        SSL_CTX* ssl_ctx_native = this->ssl_ctx.native_handle();

        if (SSL_CTX_set_min_proto_version(ssl_ctx_native, chatload::OPENSSL_MIN_PROTO_VERSION) != 1) {
            throw std::system_error(get_openssl_error(), "set_min_proto_version");
        }

        const char* cipher_list = args.cipher_list ? args.cipher_list.get().c_str() : chatload::OPENSSL_DEFAULT_CIPHER_LIST;
        if (SSL_CTX_set_cipher_list(ssl_ctx_native, cipher_list) != 1) {
            throw std::system_error(get_openssl_error(), "set_cipher_list");
        }

        if (args.ciphersuites && SSL_CTX_set_ciphersuites(ssl_ctx_native, args.ciphersuites.get().c_str()) != 1) {
            throw std::system_error(get_openssl_error(), "set_ciphersuites");
        }

        // Only load certificates if verification is enabled for some host(s)
        bool any_tls = std::any_of(args.hosts.begin(), args.hosts.end(),
                                   [](const chatload::cli::host& host) { return !host.insecure_tls; });
        if (!args.insecure_tls && any_tls) {
            if (args.ca_file || args.ca_path) {
                const char* CAfile = args.ca_file ? args.ca_file.get().c_str() : nullptr;
                const char* CApath = args.ca_path ? args.ca_path.get().c_str() : nullptr;
                if (SSL_CTX_load_verify_locations(ssl_ctx_native, CAfile, CApath) != 1) {
                    throw std::system_error(get_openssl_error(), "load_verify_locations");
                }
            }
            chatload::os::loadTrustedCerts(ssl_ctx_native);
        }

        this->writers.reserve(args.hosts.size());
        for (const auto& host : args.hosts) {
            this->writers.emplace_back(host, this->io_ctx, args.network_timeout, this->ssl_ctx, this->tcp_resolver);
        }
    }

    template<typename T>
    void for_each(T&& func) {
        std::for_each(this->writers.begin(), this->writers.end(), std::forward<T>(func));
    }

    bool all_down() noexcept {
        return std::all_of(this->writers.begin(), this->writers.end(), std::mem_fn(&tcp_writer::has_error));
    }

private:
    static std::error_code get_openssl_error() {
        return { static_cast<int>(ERR_get_error()), boost::asio::error::get_ssl_category() };
    }
};
}  // namespace network
}  // namespace chatload


#endif  // CHATLOAD_NETWORK_H
