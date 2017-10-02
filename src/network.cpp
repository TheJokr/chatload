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

// Forward declaration
#include "network.hpp"

// Streams
#include <sstream>

// Containers
#include <string>
#include <vector>
#include <unordered_set>

// C++ REST SDK (PPLX, HTTP Client, JSON)
#include <pplx/pplxtasks.h>
#include <cpprest/http_client.h>
#include <cpprest/json.h>


namespace {
std::wstring join_set(const std::unordered_set<std::wstring>& set, const std::wstring& sep) {
    if (set.empty()) { return std::wstring(); }
    std::wstringstream wss;

    auto iter = set.cbegin();
    wss << *iter;
    for (++iter; iter != set.cend(); ++iter) {
        wss << sep << *iter;
    }

    return wss.str();
}
}  // Anonymous namespace


std::vector<pplx::task<void>> chatload::network::sendNames(const web::json::array& endpoints,
                                                           const std::unordered_set<std::wstring>& names) {
    std::vector<pplx::task<void>> reqs;
    reqs.reserve(endpoints.size());
    std::wstring all_names = join_set(names, L",");

    for (const auto& endpt : endpoints) {
        std::wstring host = endpt.at(L"host").as_string();
        web::http::client::http_client client(host);

        reqs.push_back(client.request(
            web::http::methods::POST, endpt.at(L"resource").as_string(),
            endpt.at(L"parameter").as_string() + L"=" + all_names,
            L"application/x-www-form-urlencoded"
        ).then([host](web::http::http_response resp) {
            if (resp.status_code() == web::http::status_codes::OK) {
                std::wcout << "Successfully added character names to the endpoint at " << host << std::endl;
            } else {
                std::wcerr << "Failed to add character names to the endpoint at " << host << ": "
                           << "Server responded with status code " << resp.status_code() << std::endl;
            }
        }));
    }

    return reqs;
}
