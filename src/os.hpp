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

// WinAPI configuration
#define WIN32_LEAN_AND_MEAN
#define NTDDI_VERSION NTDDI_WIN7
#define _WIN32_WINNT _WIN32_WINNT_WIN7

// Header guard
#pragma once
#ifndef CHATLOAD_OS_H
#define CHATLOAD_OS_H


// Containers
#include <string>

// WinAPI
#include <Windows.h>

namespace chatload {
namespace os {
std::wstring GetDocumentsFolder();

struct dir_entry {
    std::wstring name;
    unsigned long long size;
    unsigned long long write_time;

    explicit dir_entry(const WIN32_FIND_DATAW& data);
};

class dir_list {
private:
    bool initialized = false;
    bool file_buffered;
    DWORD file_attrs;
    WIN32_FIND_DATAW data;
    HANDLE hdl;

public:
    dir_list() noexcept : initialized(false) {}
    explicit dir_list(const std::wstring& dir, bool enable_dirs = false,
                      bool enable_hidden = false, bool enable_system = false);
    dir_list(const dir_list& other) = delete;
    dir_list(dir_list&& other) noexcept;

    ~dir_list() { this->close(); }
    void close() noexcept;

    dir_list& operator=(const dir_list& other) = delete;
    dir_list& operator=(dir_list&& other) noexcept;

    bool fetch_file() noexcept;
    dir_entry get_file();
};
}  // namespace os
}  // namespace chatload


#endif  // CHATLOAD_OS_H
