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

// WinAPI configuration
#define WIN32_LEAN_AND_MEAN
#define NTDDI_VERSION NTDDI_WIN7  // NOLINT(cppcoreguidelines-macro-usage)
#define _WIN32_WINNT _WIN32_WINNT_WIN7  // NOLINT(cppcoreguidelines-macro-usage)

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
#include <ShlObj.h>

// OpenSSL
#include <openssl/ssl.h>

// chatload components
#include "common.hpp"


void chatload::os::loadTrustedCerts(SSL_CTX* ctx) noexcept {
    if (!ctx) { return; }
    // TODO: implement cert import for Windows
}


chatload::string chatload::os::getLogFolder() {
    std::wstring path;
    PWSTR winres;

    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &winres);
    if (SUCCEEDED(hr)) {
        path = winres;
        CoTaskMemFree(winres);
        path += LR"(\EVE\logs\Chatlogs)";
    }

    return path;
}


namespace {
chatload::os::dir_entry dir_entry_from_find_data(const WIN32_FIND_DATAW& data) {
    return { data.cFileName, (static_cast<DWORDLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow,
             (static_cast<DWORDLONG>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime };
}
}  // Anonymous namespace


struct chatload::os::dir_handle::iter_state {
    DWORD file_attrs;
    WIN32_FIND_DATAW find_data;
    void* find_hdl;
};


chatload::os::dir_handle::dir_handle() noexcept = default;

chatload::os::dir_handle::dir_handle(const chatload::string& dir, bool enable_dirs,
                                     bool enable_hidden, bool enable_system) :
        status(ACTIVE), state(new iter_state{ 0, {}, {} }) {
    this->state->find_hdl = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &this->state->find_data,
                                             FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if (this->state->find_hdl == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Error opening dir_handle (Code " + std::to_string(GetLastError()) + ")");
    }

    if (!enable_dirs) { this->state->file_attrs |= FILE_ATTRIBUTE_DIRECTORY; }
    if (!enable_hidden) { this->state->file_attrs |= FILE_ATTRIBUTE_HIDDEN; }
    if (!enable_system) { this->state->file_attrs |= FILE_ATTRIBUTE_SYSTEM; }

    if (this->state->find_data.dwFileAttributes & this->state->file_attrs) { this->fetch_next(); }
    else { this->cur_entry = dir_entry_from_find_data(this->state->find_data); }
}

chatload::os::dir_handle::dir_handle(chatload::os::dir_handle&& other) noexcept :
        status(std::move(other.status)), state(std::move(other.state)), cur_entry(std::move(other.cur_entry)) {
    other.status = CLOSED;
}

chatload::os::dir_handle& chatload::os::dir_handle::operator=(chatload::os::dir_handle&& other) noexcept {
    this->close();

    this->status = std::move(other.status);
    this->state = std::move(other.state);
    this->cur_entry = std::move(other.cur_entry);

    other.status = CLOSED;
    return *this;
}

chatload::os::dir_handle::~dir_handle() { this->close(); }

void chatload::os::dir_handle::close() noexcept {
    if (this->status == CLOSED) { return; }

    FindClose(this->state->find_hdl);
    this->status = CLOSED;
}

bool chatload::os::dir_handle::fetch_next() {
    if (this->status != ACTIVE) { return false; }

    // Explore files until either no more files are left
    // or a file that doesn't match file_attrs is found
    bool ok;
    while ((ok = FindNextFileW(this->state->find_hdl, &this->state->find_data)) == true &&
           (this->state->find_data.dwFileAttributes & this->state->file_attrs)) {}

    if (!ok) {
        DWORD ec = GetLastError();
        if (ec != ERROR_NO_MORE_FILES) {
            throw std::runtime_error("Error retrieving next file in dir_handle (Code " + std::to_string(ec) + ")");
        }

        this->status = EXHAUSTED;
    } else {
        this->cur_entry = dir_entry_from_find_data(this->state->find_data);
    }

    return ok;
}
