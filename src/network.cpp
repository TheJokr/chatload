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
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/ssl/rfc2818_verification.hpp>

// chatload components
#include "cli.hpp"


using namespace std::placeholders;  // NOLINT(google-build-using-namespace)

namespace asio {
using namespace boost::asio;  // NOLINT(google-build-using-namespace): effectively a namespace alias
using tcp = ip::tcp;
}  // namespace asio


chatload::network::tcp_writer::tcp_writer(const chatload::cli::host& host, asio::io_context& io_ctx,
                                          asio::ssl::context& ssl_ctx, asio::tcp::resolver& tcp_resolver) :
        io_ctx(io_ctx), ssl_sock(io_ctx, ssl_ctx), host(host) {
    boost::system::error_code ec;

    if (this->host.insecure_tls) {
        this->ssl_sock.set_verify_mode(asio::ssl::verify_none, ec);
        if (ec) { this->net_err.emplace(ec, "set_verify_mode"); return; }
    }

    this->ssl_sock.set_verify_callback(asio::ssl::rfc2818_verification(this->host.name), ec);
    if (ec) { this->net_err.emplace(ec, "set_verify_callback"); return; }

    tcp_resolver.async_resolve(this->host.name, this->host.port, asio::tcp::resolver::address_configured,
                               std::bind(&tcp_writer::resolve_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::resolve_hdlr(const boost::system::error_code& ec,
                                                 // NOLINTNEXTLINE(performance-unnecessary-value-param)
                                                 asio::tcp::resolver::results_type results) {
    if (ec) { this->net_err.emplace(ec, "async_resolve"); return; }
    asio::async_connect(this->ssl_sock.next_layer(), results, std::bind(&tcp_writer::connect_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::connect_hdlr(const boost::system::error_code& ec, const asio::tcp::endpoint&) {
    if (ec) { this->net_err.emplace(ec, "async_connect"); return; }
    this->ssl_sock.async_handshake(decltype(this->ssl_sock)::client,
                                   std::bind(&tcp_writer::ssl_handshake_hdlr, this, _1));
}

void chatload::network::tcp_writer::ssl_handshake_hdlr(const boost::system::error_code& ec) {
    if (ec) {
        this->net_err.emplace(ec, "async_handshake");
        this->ssl_shutdown_hdlr({});  // shutdown stream (TLS not established)
        return;
    }

    this->write_queued = false;
}


void chatload::network::tcp_writer::write_hdlr(const boost::system::error_code& ec, std::size_t) {
    this->write_queued = false;
    if (ec) {
        this->net_err.emplace(ec, "async_write");
        this->buffers.clear();  // stop further write attempts
        this->shutdown_queued = true;  // shutdown stream
    }

    if (this->buffers.empty()) {
        if (this->shutdown_queued) {
            this->ssl_sock.async_shutdown(std::bind(&tcp_writer::ssl_shutdown_hdlr, this, _1));
            this->write_queued = true;
        }
        return;
    }

    asio::async_write(this->ssl_sock, this->buffers, std::bind(&tcp_writer::write_hdlr, this, _1, _2));
    this->write_queued = true;
    this->buffers.clear();
}


void chatload::network::tcp_writer::ssl_shutdown_hdlr(const boost::system::error_code& ec) {
    // Don't return on errors, shutdown stream instead
    if (ec && !this->net_err) { this->net_err.emplace(ec, "async_shutdown"); }

    // Close stream after TLS shutdown
    boost::system::error_code ec_tcp;
    auto& tcp_sock = this->ssl_sock.next_layer();

    tcp_sock.shutdown(asio::tcp::socket::shutdown_both, ec_tcp);
    if (ec_tcp && !this->net_err) { this->net_err.emplace(ec_tcp, "shutdown"); }

    tcp_sock.close(ec_tcp);
    if (ec_tcp && !this->net_err) { this->net_err.emplace(ec_tcp, "close"); }
}
