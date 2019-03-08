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

namespace chatload {
inline namespace constants {
constexpr char VERSION[] = "2.0.0-dev";
constexpr wchar_t CONFIGFILE[] = L"chatload.cfg";
constexpr char CONFIG_HELP[] = "chatload.cfg";
constexpr wchar_t CACHEFILE[] = L"filecache.tsv";
constexpr char CACHE_HELP[] = "filecache.tsv";
constexpr wchar_t DEFAULTCONFIG[] = LR"({"POST": [{"host": "https://api.dashsec.com", "resource": "/charDump.php", )"
                                    LR"("parameter": "name"}], "regex": ".*"})";
constexpr wchar_t DEFAULTHOST[] = L"api.dashsec.com";
constexpr std::uint_least16_t DEFAULTPORT = 36643;
}  // namespace constants
}  // namespace chatload


#endif  // CHATLOAD_CONSTANTS_H
