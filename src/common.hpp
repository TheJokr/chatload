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
#ifndef CHATLOAD_COMMON_H
#define CHATLOAD_COMMON_H


// Streams
#include <iostream>

// Containers
#include <string>

namespace chatload {
#ifdef _WIN32
using char_t = wchar_t;
#define CHATLOAD_STRING(str) L##str
#define CHATLOAD_COUT ::std::wcout
#define CHATLOAD_CIN ::std::wcin
#define CHATLOAD_CERR ::std::wcerr
#define CHATLOAD_PATH_SEP CHATLOAD_STRING('\\')
#else  // !_WIN32
using char_t = char;
#define CHATLOAD_STRING(str) str
#define CHATLOAD_COUT ::std::cout
#define CHATLOAD_CIN ::std::cin
#define CHATLOAD_CERR ::std::cerr
#define CHATLOAD_PATH_SEP CHATLOAD_STRING('/')
#endif  // _WIN32

using string = std::basic_string<char_t>;
}  // namespace chatload


#endif  // CHATLOAD_COMMON_H
