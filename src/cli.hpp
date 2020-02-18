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
#ifndef CHATLOAD_CLI_H
#define CHATLOAD_CLI_H


// Containers
#include <string>
#include <vector>

// Boost
#include <boost/optional.hpp>

// chatload components
#include "common.hpp"

namespace chatload {
namespace cli {
struct host {
    // See options::insecure_tls. If false, use the global setting.
    bool insecure_tls = false;

    // Hostname or IP address
    std::string name;

    // Numerical port or service name (e.g., "http")
    std::string port;
};

struct options {
    // Verbose mode: report individual files
    bool verbose = false;

    // Use write time cache for files
    bool use_cache = true;

    // Disable TLS certificate verification
    // May be overwritten for individual hosts
    bool insecure_tls = false;

    // File/directory with OpenSSL's CAfile/CApath format
    // To be passed to SSL_CTX_load_verify_locations
    boost::optional<std::string> ca_file, ca_path;

    // TLSv1.2/TLSv1.3 ciphers to use
    boost::optional<std::string> cipher_list, ciphersuites;

    // Regex to filter log filenames
    chatload::string regex;

    // Location of write time cache, defaults to chatload::CACHE_FILE in OS-native cache folder
    boost::optional<chatload::string> cache_file;

    // Location of log files, defaults to the equivalent of ~/Documents/EVE/logs/Chatlogs
    boost::optional<chatload::string> log_folder;

    // chatload API servers to upload results to
    std::vector<host> hosts;
};

// NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
options parseArgs(int argc, chatload::char_t* argv[]);
}  // namespace cli
}  // namespace chatload


#endif  // CHATLOAD_CLI_H
