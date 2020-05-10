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
#include "network.hpp"

// Utility
#include <functional>

// Boost.System
#include <boost/system/error_code.hpp>

// Boost.Asio
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/ssl/rfc2818_verification.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

// chatload components
#include "constants.hpp"
#include "error.hpp"
#include "cli.hpp"


using namespace std::placeholders;  // NOLINT(google-build-using-namespace)

namespace asio {
using namespace boost::asio;  // NOLINT(google-build-using-namespace): effectively a namespace alias
using tcp = ip::tcp;
}  // namespace asio


namespace {
template<typename T>
auto buffer_single(T* ptr) noexcept {
    static_assert(std::is_trivially_copyable<T>::value && !std::is_array<T>::value,
                  "T must be a trivially-copyable non-array type");
    return asio::buffer(ptr, sizeof(T));
}
}  // Anonymous namespace


chatload::network::tcp_writer::tcp_writer(const chatload::cli::host& host, asio::io_context& io_ctx,
                                          const boost::posix_time::time_duration& timeout,
                                          asio::ssl::context& ssl_ctx, asio::tcp::resolver& tcp_resolver) :
        io_ctx(io_ctx), timeout_timer(io_ctx), ssl_sock(io_ctx, ssl_ctx), host(host) {
    boost::system::error_code ec;

    if (this->host.insecure_tls) {
        this->ssl_sock.set_verify_mode(asio::ssl::verify_none, ec);
        if (ec) { this->net_err.emplace(ec, "set_verify_mode"); return; }
    }

    this->ssl_sock.set_verify_callback(asio::ssl::rfc2818_verification(this->host.name), ec);
    if (ec) { this->net_err.emplace(ec, "set_verify_callback"); return; }

    this->timeout_timer.expires_from_now(timeout, ec);
    if (ec) { this->net_err.emplace(ec, "expires_from_now"); return; }
    this->timeout_timer.async_wait(std::bind(&tcp_writer::timeout_hdlr, this, _1));

    tcp_resolver.async_resolve(this->host.name, this->host.port, asio::tcp::resolver::address_configured,
                               std::bind(&tcp_writer::resolve_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::resolve_hdlr(const boost::system::error_code& ec,
                                                 // NOLINTNEXTLINE(performance-unnecessary-value-param)
                                                 asio::tcp::resolver::results_type results) {
    if (ec) {
        if (ec != asio::error::operation_aborted) { this->abort_err(ec, "async_resolve"); }
        return;
    }
    asio::async_connect(this->ssl_sock.next_layer(), results, std::bind(&tcp_writer::connect_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::connect_hdlr(const boost::system::error_code& ec, const asio::tcp::endpoint&) {
    if (ec) {
        if (ec != asio::error::operation_aborted) { this->abort_err(ec, "async_connect"); }
        return;
    }

    // Enable TCP_NODELAY for TLS handshake and version exchange
    boost::system::error_code tcp_ec;
    this->ssl_sock.next_layer().set_option(asio::tcp::no_delay(true), tcp_ec);
    if (tcp_ec) {
        this->net_err.emplace(tcp_ec, "set_no_delay");
        this->ssl_shutdown_hdlr({});  // shut down stream (TLS not established)
        return;
    }

    this->ssl_sock.async_handshake(decltype(this->ssl_sock)::client,
                                   std::bind(&tcp_writer::ssl_handshake_hdlr, this, _1));
}

void chatload::network::tcp_writer::ssl_handshake_hdlr(const boost::system::error_code& ec) {
    if (ec) {
        if (ec != asio::error::operation_aborted) {
            this->net_err.emplace(ec, "async_handshake");
            this->ssl_shutdown_hdlr({});  // shut down stream (TLS not established)
        }
        return;
    }

    // The version exchange handler takes care of disabling write_queued
    asio::async_write(this->ssl_sock, buffer_single(&chatload::protocol::VERSION), boost::asio::detached);
    asio::async_read(this->ssl_sock, buffer_single(&this->server_buf),
                     std::bind(&tcp_writer::version_exchange_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::version_exchange_hdlr(const boost::system::error_code& ec, std::size_t) {
    std::error_code app_ec;

    if (ec) {
        if (ec == asio::error::operation_aborted) { return; }
        app_ec = ec;
    } else switch (this->server_buf) {  // NOLINT(readability-braces-around-statements,google-readability-braces-around-statements)
        case chatload::protocol::command::VERSION_OK: {
            // Disable TCP_NODELAY again after version exchange
            boost::system::error_code tcp_ec;
            this->ssl_sock.next_layer().set_option(asio::tcp::no_delay(false), tcp_ec);
            if (tcp_ec) { app_ec = tcp_ec; break; }

            // Abort wait operation on timeout_timer to stop io_context
            this->timeout_timer.cancel(tcp_ec);
            if (tcp_ec) { app_ec = tcp_ec; break; }

            this->write_queued = false;
            return;  // all other paths are exceptional and are handled together below
        }

        case chatload::protocol::command::VERSION_NOT_SUPPORTED:
            app_ec = chatload::error::PROTOCOL_VERSION_NOT_SUPPORTED;
            break;
        default:
            app_ec = chatload::error::UNKNOWN_COMMAND;
            break;
    }

    this->net_err.emplace(app_ec, "version exchange");
    this->shutdown_immediate();
}


void chatload::network::tcp_writer::start_after_init() {
    if (this->net_err) { return; }

    // Restart wait operation on timeout_timer
    this->timeout_timer.async_wait(std::bind(&tcp_writer::timeout_hdlr, this, _1));

    // Restart server command read loop
    asio::async_read(this->ssl_sock, buffer_single(&this->server_buf),
                     std::bind(&tcp_writer::read_hdlr, this, _1, _2));
}


void chatload::network::tcp_writer::read_hdlr(const boost::system::error_code& ec, std::size_t) {
    if (ec) {
        // Silently ignore aborts and truncations (the latter are handled by write_hdlr)
        if (ec == asio::error::operation_aborted || ec == asio::ssl::error::stream_truncated) { return; }

        std::error_code app_ec;
        if (ec == asio::error::eof) {
            // Server sent TLS close_notify
            app_ec = chatload::error::SERVER_SHUTDOWN;
        } else {
            // Unexpected error
            app_ec = ec;
        }

        this->net_err.emplace(app_ec, "async_read");
        this->shutdown_immediate();
        return;
    }

    // Commands received after the protocol exchange are currently ignored
    asio::async_read(this->ssl_sock, buffer_single(&this->server_buf),
                     std::bind(&tcp_writer::read_hdlr, this, _1, _2));
}


void chatload::network::tcp_writer::write_hdlr(const boost::system::error_code& ec, std::size_t) {
    if (ec) {
        // Silently ignore aborts (e.g., because of shutdown)
        if (ec == asio::error::operation_aborted) { return; }

        // Anything else is unexpected (e.g., stream_truncated)
        this->net_err.emplace(ec, "async_write");
        this->shutdown_immediate();
        return;
    }

    this->write_queued = false;
    if (this->buffers.empty()) {
        if (this->shutdown_status == SHUTDOWN_REQUESTED) { this->shutdown_immediate(); }
        return;
    }

    this->write_queued = true;
    asio::async_write(this->ssl_sock, this->buffers, std::bind(&tcp_writer::write_hdlr, this, _1, _2));
    this->buffers.clear();
}


void chatload::network::tcp_writer::shutdown_immediate() {
    boost::system::error_code ec;
    this->ssl_sock.next_layer().cancel(ec);  // prevent cancel from throwing

    this->write_queued = true;
    this->shutdown_status = SHUTDOWN_IN_PROGRESS;
    this->ssl_sock.async_shutdown(std::bind(&tcp_writer::ssl_shutdown_hdlr, this, _1));
}


void chatload::network::tcp_writer::ssl_shutdown_hdlr(const boost::system::error_code& ec) {
    if (ec) {
        // Silently ignore aborts (e.g., because of timeout)
        if (ec == asio::error::operation_aborted) { return; }

        // Otherwise, only stream_truncated and eof errors are allowed.
        // See https://stackoverflow.com/a/25703699.
        if (ec != asio::ssl::error::stream_truncated && ec != asio::error::eof && !this->net_err) {
            this->net_err.emplace(ec, "async_shutdown");
        }

        // Don't return, shut down stream instead
    }

    // Stop timeout_timer since remaining operations are synchronous
    boost::system::error_code tcp_ec;
    this->timeout_timer.cancel(tcp_ec);  // prevent cancel from throwing

    // Close stream after TLS shutdown
    auto& tcp_sock = this->ssl_sock.next_layer();

    tcp_sock.shutdown(asio::tcp::socket::shutdown_both, tcp_ec);
    if (tcp_ec && !this->net_err) { this->net_err.emplace(tcp_ec, "shutdown"); }

    tcp_sock.close(tcp_ec);
    if (tcp_ec && !this->net_err) { this->net_err.emplace(tcp_ec, "close"); }
}


void chatload::network::tcp_writer::timeout_hdlr(const boost::system::error_code& ec) {
    if (ec) {
        if (ec == asio::error::operation_aborted) { return; }

        this->net_err.emplace(ec, "async_wait");
        this->shutdown_immediate();
        return;
    }

    boost::system::error_code tcp_ec;
    this->ssl_sock.next_layer().cancel(tcp_ec);  // prevent cancel from throwing

    this->write_queued = true;
    this->shutdown_status = SHUTDOWN_IN_PROGRESS;
    this->net_err.emplace(chatload::error::WRITER_TIMEOUT);
    this->ssl_shutdown_hdlr({});
}
