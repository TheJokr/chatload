/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2018  Leo Blöcher
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
#ifndef CHATLOAD_NETWORK_H
#define CHATLOAD_NETWORK_H


// Containers
#include <string>
#include <vector>
#include <unordered_set>

// C++ REST SDK (PPLX, JSON)
#include <pplx/pplxtasks.h>
#include <cpprest/json.h>

namespace chatload {
namespace network {
std::vector<pplx::task<void>> sendNames(const web::json::array& endpoints,
                                        const std::unordered_set<std::wstring>& names);
}  // namespace network
}  // namespace chatload


#endif  // CHATLOAD_NETWORK_H
