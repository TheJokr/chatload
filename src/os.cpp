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

// Forward declaration
#include "os.hpp"

// Containers
#include <vector>

// Exceptions
#include <system_error>

// chatload components
#include "common.hpp"
#include "deref_proxy.hpp"


std::error_code chatload::os::createPath(const chatload::string& path) {
    const auto final_pos = path.rfind(chatload::PATH_SEP);
    if (final_pos == chatload::string::npos) { return {}; }

    // Ignore the first path segment for absolute paths
    // This is the drive letter on Windows and empty on OSX
    auto pos = path.find(chatload::PATH_SEP);
    if (chatload::os::internal::is_path_absolute(path)) {
        pos = path.find(chatload::PATH_SEP, pos + 1);
    }
    if (pos == chatload::string::npos) { return {}; }

    // Create a writeable C-string of the directories in the path
    // This avoids repeated copies when calling mkdir in the loop
    std::vector<chatload::char_t> dirpath(path.begin(), path.begin() + final_pos + 1);
    dirpath[final_pos] = CHATLOAD_STRING('\0');

    for (; pos <= final_pos; pos = path.find(chatload::PATH_SEP, pos + 1)) {
        // Virtually terminate the directory path at the current directory
        dirpath[pos] = CHATLOAD_STRING('\0');

        // NOLINTNEXTLINE(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): permission mask
        std::error_code ec = chatload::os::internal::mkdir(dirpath.data(), 0755);
        if (ec && ec != std::errc::file_exists) { return ec; }

        // Restore full directory path
        dirpath[pos] = chatload::PATH_SEP;
    }

    return {};
}


chatload::os::dir_handle::iterator chatload::os::dir_handle::begin() noexcept {
    return chatload::os::dir_handle::iterator(this);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): iterator factories are generally not static
chatload::os::dir_handle::iterator chatload::os::dir_handle::end() noexcept {
    return chatload::os::dir_handle::iterator();
}


chatload::os::dir_iter::dir_iter(chatload::os::dir_handle* hdl_ref) noexcept : hdl_ref(hdl_ref) {}

chatload::os::dir_iter& chatload::os::dir_iter::operator++() {
    if (this->hdl_ref) { this->hdl_ref->fetch_next(); }
    return *this;
}

chatload::deref_proxy<chatload::os::dir_iter::value_type> chatload::os::dir_iter::operator++(int) {
    chatload::deref_proxy<value_type> res(this->hdl_ref ? this->hdl_ref->cur_entry : value_type{});
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
    // NOLINTNEXTLINE(readability-implicit-bool-conversion): false positive (pointer conversion is allowed)
    return !this->hdl_ref || (this->hdl_ref->status == chatload::os::dir_handle::EXHAUSTED);
}


bool chatload::os::operator==(const dir_iter& lhs, const dir_iter& rhs) noexcept {
    // 2 dir_iters compare equal iff both point to
    // the same dir_handle or both dir_handle's are exhausted
    return (lhs.hdl_ref == rhs.hdl_ref) || (lhs.handle_exhausted() && rhs.handle_exhausted());
}

bool chatload::os::operator!=(const dir_iter& lhs, const dir_iter& rhs) noexcept { return !(lhs == rhs); }
