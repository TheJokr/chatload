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
#ifndef CHATLOAD_CONSTANTS_H
#define CHATLOAD_CONSTANTS_H


// C headers
#include <cstdint>

// OpenSSL
#include <openssl/tls1.h>

// chatload components
#include "common.hpp"

namespace chatload {
inline namespace constants {
// Config/CLI defaults
constexpr chatload::char_t CONFIG_FILE[] = CHATLOAD_STRING("chatload.cfg");  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
constexpr char CONFIG_HELP[] = "chatload.cfg";  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)

// Chatload protocol
namespace protocol {
// Version numbering to support future protocol evolution
constexpr std::uint32_t VERSION = 1;

// Commands sent by the server
enum class command : std::uint32_t {
    NONE = 0,  // for default initialization
    VERSION_OK = 1,
    VERSION_NOT_SUPPORTED = 2
};
}  // namespace protocol

// Network defaults
constexpr char DEFAULT_HOST[] = "chatload.bloecher.dev";  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
constexpr char DEFAULT_PORT[] = "36643";  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)

// OpenSSL defaults
constexpr int OPENSSL_MIN_PROTO_VERSION = TLS1_2_VERSION;
constexpr char OPENSSL_DEFAULT_CIPHER_LIST[] = "HIGH:!eNULL:!aNULL:!kRSA:!SHA1:!MD5";  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
// constexpr char OPENSSL_DEFAULT_CIPHERSUITES[] = "TLSv1.3 defaults are fine";  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
}  // namespace constants
}  // namespace chatload


#endif  // CHATLOAD_CONSTANTS_H
