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


// C headers
#include <cstdint>

// Containers
#include <string>
#include <vector>

// Boost
#include <boost/optional.hpp>

// chatload components
#include "common.hpp"
#include "constants.hpp"

namespace chatload {
namespace cli {
struct host {
    std::string name;
    // Boost.Asio expects port as a string
    std::string port;
};

struct options {
    bool verbose = false;
    bool use_cache = true;
    chatload::string regex;
    chatload::string cache_file = chatload::CACHEFILE;
    boost::optional<chatload::string> log_folder;
    std::vector<host> hosts;
};

options parseArgs(int argc, chatload::char_t* argv[]);
}  // namespace cli
}  // namespace chatload


#endif  // CHATLOAD_CLI_H
