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

// Forward declaration
#include "os.hpp"

// Containers
#include <string>

// Exceptions
#include <stdexcept>

// Utility
#include <utility>

// WinAPI
#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <ShlObj.h>


bool chatload::os::SetTerminalTitle(const wchar_t* title) noexcept {
    return SetConsoleTitleW(title);
}

std::wstring chatload::os::GetDocumentsFolder() {
    std::wstring path;
    PWSTR winres;

    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &winres);
    if (SUCCEEDED(hr)) {
        path = winres;
        CoTaskMemFree(winres);
    }

    return path;
}


chatload::os::wargs::wargs() {
    this->argv = CommandLineToArgvW(GetCommandLineW(), &this->argc);
    if (this->argv == NULL) {
        throw std::runtime_error("Error parsing command line (Code " + std::to_string(GetLastError()) + ")");
    }
}

chatload::os::wargs::~wargs() {
    LocalFree(this->argv);
}


chatload::os::dir_entry::dir_entry(const WIN32_FIND_DATAW& data) {
    this->name = data.cFileName;
    this->size = (static_cast<DWORDLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    this->write_time = (static_cast<DWORDLONG>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                        data.ftLastWriteTime.dwLowDateTime;
}


chatload::os::dir_list::dir_list(const std::wstring& dir, bool enable_dirs, bool enable_hidden, bool enable_system) {
    this->hdl = FindFirstFileExW((dir + L'*').c_str(), FindExInfoBasic, &this->data,
                                 FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if (this->hdl == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Error opening dir_list (Code " + std::to_string(GetLastError()) + ")");
    }

    this->initialized = true;
    this->file_attrs = 0;
    if (!enable_dirs) { this->file_attrs |= FILE_ATTRIBUTE_DIRECTORY; }
    if (!enable_hidden) { this->file_attrs |= FILE_ATTRIBUTE_HIDDEN; }
    if (!enable_system) { this->file_attrs |= FILE_ATTRIBUTE_SYSTEM; }
    this->file_buffered = !(this->data.dwFileAttributes & this->file_attrs);
}

chatload::os::dir_list::dir_list(chatload::os::dir_list&& other) noexcept :
        initialized(std::move(other.initialized)), file_buffered(std::move(other.file_buffered)),
        file_attrs(std::move(other.file_attrs)), data(std::move(other.data)), hdl(std::move(other.hdl)) {
    other.initialized = false;
}

void chatload::os::dir_list::close() noexcept {
    if (!this->initialized) { return; }

    FindClose(this->hdl);
    this->initialized = false;
}

chatload::os::dir_list& chatload::os::dir_list::operator=(chatload::os::dir_list&& other) noexcept {
    this->close();

    this->initialized = std::move(other.initialized);
    this->file_buffered = std::move(other.file_buffered);
    this->file_attrs = std::move(other.file_attrs);
    this->data = std::move(other.data);
    this->hdl = std::move(other.hdl);

    other.initialized = false;
    return *this;
}

bool chatload::os::dir_list::fetch_file() noexcept {
    if (!this->initialized) { return false; }
    if (this->file_buffered) { return true; }

    // Explore files until either no more files are left
    // or a file that doesn't match file_attrs is found
    while ((this->file_buffered = FindNextFileW(this->hdl, &this->data)) == true &&
           (this->data.dwFileAttributes & this->file_attrs)) {}

    return this->file_buffered;
}

chatload::os::dir_entry chatload::os::dir_list::get_file() {
    if (!this->fetch_file()) {
        throw std::runtime_error("Error retrieving next file in dir_list");
    }
    this->file_buffered = false;

    return chatload::os::dir_entry(this->data);
}
