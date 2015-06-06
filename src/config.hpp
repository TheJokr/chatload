/*
 * chatload: collect EVE Online character names from chat logs
 * Copyright (C) 2015  Leo Blöcher
 *
 * This file is part of chatload.
 *
 * chatload is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * chatload is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with chatload.  If not, see <http://www.gnu.org/licenses/>.
 */


// Header guard
#pragma once
#ifndef CHATLOAD_CONFIG_H
#define CHATLOAD_CONFIG_H


// Containers
#include <string>

// Casablanca (JSON)
#include <cpprest\json.h>


namespace chatload {
    class config {
        private:
            std::wstring cfgFilename;
            web::json::value cfgObj;
        public:
            config(const std::wstring& filename = L"config.json");
            bool load(const std::wstring& filename);
            bool save();
            bool reload();
            web::json::value get(const std::wstring& path);
            bool set(const std::wstring& path, const std::wstring& content);
    };
}


#endif // CHATLOAD_CONFIG_H
