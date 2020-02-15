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
#include <boost/system/system_error.hpp>

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


using namespace std::placeholders;

namespace asio {
using namespace boost::asio;
using tcp = ip::tcp;
}  // namespace asio


chatload::network::tcp_writer::tcp_writer(const chatload::cli::host &host, asio::io_context& io_ctx,
                                          asio::ssl::context& ssl_ctx, asio::tcp::resolver& tcp_resolver) :
        io_ctx(io_ctx), ssl_sock(io_ctx, ssl_ctx) {
    if (host.insecure_tls) { this->ssl_sock.set_verify_mode(asio::ssl::verify_none); }
    this->ssl_sock.set_verify_callback(asio::ssl::rfc2818_verification(host.name));

    tcp_resolver.async_resolve(host.name, host.port, asio::tcp::resolver::address_configured,
                               std::bind(&tcp_writer::resolve_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::resolve_hdlr(const boost::system::error_code& ec,
                                                 asio::tcp::resolver::results_type results) {
    if (ec) { throw boost::system::system_error(ec, "async_resolve"); }
    asio::async_connect(this->ssl_sock.next_layer(), results, std::bind(&tcp_writer::connect_hdlr, this, _1, _2));
}

void chatload::network::tcp_writer::connect_hdlr(const boost::system::error_code& ec, const asio::tcp::endpoint&) {
    if (ec) { throw boost::system::system_error(ec, "async_connect"); }
    this->ssl_sock.async_handshake(decltype(this->ssl_sock)::client,
                                   std::bind(&tcp_writer::ssl_handshake_hdlr, this, _1));
}

void chatload::network::tcp_writer::ssl_handshake_hdlr(const boost::system::error_code& ec) {
    if (ec) { throw boost::system::system_error(ec, "async_handshake"); }
    this->write_queued = false;
}


void chatload::network::tcp_writer::write_hdlr(const boost::system::error_code& ec, std::size_t) {
    this->write_queued = false;
    if (ec) { throw boost::system::system_error(ec, "async_write"); }

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
    if (ec) { throw boost::system::system_error(ec, "async_shutdown"); }

    // Close after SSL shutdown
    auto& tcp_sock = this->ssl_sock.next_layer();
    tcp_sock.shutdown(asio::tcp::socket::shutdown_both);
    tcp_sock.close();
}
