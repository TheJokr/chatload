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
#define NTDDI_VERSION NTDDI_WIN7
#define _WIN32_WINNT _WIN32_WINNT_WIN7

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

// chatload components
#include "deref_proxy.hpp"


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


chatload::os::dir_entry::dir_entry(const WIN32_FIND_DATAW& data) : name(data.cFileName),
        size((static_cast<DWORDLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow),
        write_time((static_cast<DWORDLONG>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                   data.ftLastWriteTime.dwLowDateTime) {}


chatload::os::dir_handle::dir_handle() noexcept : status(CLOSED) {}

chatload::os::dir_handle::dir_handle(const std::wstring& dir, bool enable_dirs,
                                     bool enable_hidden, bool enable_system) :
        status(ACTIVE), file_attrs(0), find_data(new WIN32_FIND_DATAW) {
    this->find_hdl = FindFirstFileExW((dir + L'*').c_str(), FindExInfoBasic, this->find_data.get(),
                                      FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if (this->find_hdl == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Error opening dir_handle (Code " + std::to_string(GetLastError()) + ")");
    }

    if (!enable_dirs) { this->file_attrs |= FILE_ATTRIBUTE_DIRECTORY; }
    if (!enable_hidden) { this->file_attrs |= FILE_ATTRIBUTE_HIDDEN; }
    if (!enable_system) { this->file_attrs |= FILE_ATTRIBUTE_SYSTEM; }

    if (this->find_data->dwFileAttributes & this->file_attrs) { this->fetch_next(); }
    else { this->cur_entry = *(this->find_data); }
}

chatload::os::dir_handle::dir_handle(chatload::os::dir_handle&& other) noexcept :
        status(std::move(other.status)), file_attrs(std::move(other.file_attrs)),
        cur_entry(std::move(other.cur_entry)), find_data(std::move(other.find_data)),
        find_hdl(std::move(other.find_hdl)) {
    other.status = CLOSED;
}

chatload::os::dir_handle& chatload::os::dir_handle::operator=(chatload::os::dir_handle&& other) noexcept {
    this->close();

    this->status = std::move(other.status);
    this->file_attrs = std::move(other.file_attrs);
    this->cur_entry = std::move(other.cur_entry);
    this->find_data = std::move(other.find_data);
    this->find_hdl = std::move(other.find_hdl);

    other.status = CLOSED;
    return *this;
}

chatload::os::dir_handle::~dir_handle() { this->close(); }

void chatload::os::dir_handle::close() noexcept {
    if (this->status == CLOSED) { return; }

    FindClose(this->find_hdl);
    this->status = CLOSED;
}

chatload::os::dir_handle::iterator chatload::os::dir_handle::begin() noexcept {
    return chatload::os::dir_handle::iterator(this);
}

chatload::os::dir_handle::iterator chatload::os::dir_handle::end() noexcept {
    return chatload::os::dir_handle::iterator();
}

bool chatload::os::dir_handle::fetch_next() {
    if (this->status != ACTIVE) { return false; }

    // Explore files until either no more files are left
    // or a file that doesn't match file_attrs is found
    bool ok;
    while ((ok = FindNextFileW(this->find_hdl, this->find_data.get())) == true &&
           (this->find_data->dwFileAttributes & this->file_attrs)) {}

    if (!ok) {
        DWORD ec = GetLastError();
        if (ec != ERROR_NO_MORE_FILES) {
            throw std::runtime_error("Error retrieving next file in dir_handle (Code " + std::to_string(ec) + ")");
        }

        this->status = EXHAUSTED;
    } else {
        this->cur_entry = *(this->find_data);
    }

    return ok;
}


chatload::os::dir_iter::dir_iter(chatload::os::dir_handle* hdl_ref) noexcept : hdl_ref(hdl_ref) {}

chatload::os::dir_iter& chatload::os::dir_iter::operator++() {
    if (this->hdl_ref) { this->hdl_ref->fetch_next(); }
    return *this;
}

chatload::deref_proxy<chatload::os::dir_iter::value_type> chatload::os::dir_iter::operator++(int) {
    chatload::deref_proxy<dir_iter::value_type> res({});
    if (this->hdl_ref) {
        res = chatload::deref_proxy<dir_iter::value_type>(this->hdl_ref->cur_entry);
    }

    ++(*this);
    return res;
}

chatload::os::dir_iter::reference chatload::os::dir_iter::operator*() noexcept {
    return this->hdl_ref->cur_entry;
}

chatload::os::dir_iter::pointer chatload::os::dir_iter::operator->() noexcept {
    return &this->hdl_ref->cur_entry;
}

bool chatload::os::dir_iter::handle_exhausted() const noexcept {
    // dir_handle is considered exhausted for end sentinel (hdl_ref == nullptr)
    return !this->hdl_ref || (this->hdl_ref->status == chatload::os::dir_handle::EXHAUSTED);
}


bool chatload::os::operator==(const dir_iter& lhs, const dir_iter& rhs) noexcept {
    // 2 dir_iters compare equal iff both point to
    // the same dir_handle or both dir_handle's are exhausted
    return (lhs.hdl_ref == rhs.hdl_ref) || (lhs.handle_exhausted() && rhs.handle_exhausted());
}

bool chatload::os::operator!=(const dir_iter& lhs, const dir_iter& rhs) noexcept { return !(lhs == rhs); }
