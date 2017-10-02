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
#ifndef CHATLOAD_FILECACHE_H
#define CHATLOAD_FILECACHE_H


// Containers
#include <string>
#include <unordered_map>

namespace chatload {
namespace file_cache {
typedef std::unordered_map<std::wstring, unsigned long long> type;
bool save_to_file(const type& cache, const std::wstring& file);
type load_from_file(const std::wstring& file);
}  // namespace file_cache
}  // namespace chatload


#endif  // CHATLOAD_FILECACHE_H
