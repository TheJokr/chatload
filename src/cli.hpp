/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2017  Leo Blöcher
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

// chatload components
#include "constants.hpp"

namespace chatload {
namespace cli {
struct options {
    bool verbose = false;
    bool use_cache = true;
    std::wstring config_file = chatload::CONFIGFILE;
    std::wstring cache_file = chatload::CACHEFILE;
    std::wstring log_folder;
};

options parseArgs(int argc, wchar_t* argv[]);
}  // namespace cli
}  // namespace chatload


#endif  // CHATLOAD_CLI_H
