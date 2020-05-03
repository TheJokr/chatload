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
#ifndef CHATLOAD_ERROR_H
#define CHATLOAD_ERROR_H


// C headers
#include <cstddef>

// Containers
#include <string>

// Exceptions
#include <system_error>

// Utility
#include <type_traits>

// LZ4 compression
#include <lz4frame.h>

namespace chatload {
// Chatload error codes
enum class error : int {
    PROTOCOL_VERSION_NOT_SUPPORTED = 1,
    UNKNOWN_COMMAND = 2,
    SERVER_SHUTDOWN = 3,
    WRITER_TIMEOUT = 4
};

class error_category : public std::error_category {
public:
    const char* name() const noexcept override { return "chatload"; }

    std::string message(int code) const noexcept override {
        switch (static_cast<error>(code)) {
            case error::PROTOCOL_VERSION_NOT_SUPPORTED:
                return "Server does not support this client's version of the chatload protocol";

            case error::UNKNOWN_COMMAND:
                return "Server sent a command that is not part of the negotiated chatload protocol";

            case error::SERVER_SHUTDOWN:
                return "Server initiated a connection shutdown mid-stream";

            case error::WRITER_TIMEOUT:
                return "Connection timeout exceeded";

            default:
                return "Unknown error code";
        }
    }
};

inline std::error_code make_error_code(error code) noexcept {
    static error_category instance;
    return { static_cast<int>(code), instance };
}


// LZ4F system_error integration (used by compressor.hpp)
class LZ4F_error_category : public std::error_category {
private:
    // Copied from lz4frame.c
    static_assert(sizeof(std::ptrdiff_t) >= sizeof(LZ4F_errorCode_t),
                  "std::ptrdiff_t is not large enough to convert int to LZ4F_errorCode_t");
    static constexpr LZ4F_errorCode_t getErrorT(int code) noexcept {
        return static_cast<LZ4F_errorCode_t>(-static_cast<std::ptrdiff_t>(code));
    }

public:
    const char* name() const noexcept override { return "LZ4F"; }
    std::string message(int code) const override { return LZ4F_getErrorName(getErrorT(code)); }
};
}  // namespace chatload


namespace std {
template<>
struct is_error_code_enum<chatload::error> : public std::true_type {};

template<>
struct is_error_code_enum<LZ4F_errorCodes> : public std::true_type {};
}  // namespace std


// ADL requires this declaration to be outside the chatload namespace
inline std::error_code make_error_code(LZ4F_errorCodes code) noexcept {
    static chatload::LZ4F_error_category instance;
    return { code, instance };
}


#endif  // CHATLOAD_ERROR_H
