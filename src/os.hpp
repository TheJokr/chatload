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
#ifndef CHATLOAD_OS_H
#define CHATLOAD_OS_H


// C headers
#include <cstdint>

// Containers
#include <string>

// Utility
#include <memory>
#include <iterator>

// chatload components
#include "deref_proxy.hpp"

// Forward declaration to avoid inclusion of Windows.h
extern "C" typedef struct _WIN32_FIND_DATAW WIN32_FIND_DATAW;

namespace chatload {
namespace os {
std::wstring GetDocumentsFolder();

struct dir_entry {
    std::wstring name;
    std::uint_least64_t size;
    std::uint_least64_t write_time;

    dir_entry() = default;
    dir_entry(const WIN32_FIND_DATAW& find_data);
};

class dir_handle {
private:
    enum { CLOSED, ACTIVE, EXHAUSTED } status = CLOSED;
    std::uint_least32_t file_attrs;
    dir_entry cur_entry;
    std::unique_ptr<WIN32_FIND_DATAW> find_data;
    void* find_hdl;

    bool fetch_next();

public:
    friend class dir_iter;
    using iterator = dir_iter;

    dir_handle() noexcept;
    explicit dir_handle(const std::wstring& dir, bool enable_dirs = false,
                        bool enable_hidden = false, bool enable_system = false);
    dir_handle(const dir_handle& other) = delete;
    dir_handle(dir_handle&& other) noexcept;

    dir_handle& operator=(const dir_handle& other) = delete;
    dir_handle& operator=(dir_handle&& other) noexcept;

    ~dir_handle();
    void close() noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
};

class dir_iter {
private:
    dir_handle* const hdl_ref;

    bool handle_exhausted() const noexcept;

    friend bool operator==(const dir_iter& lhs, const dir_iter& rhs) noexcept;
    friend bool operator!=(const dir_iter& lhs, const dir_iter& rhs) noexcept;

public:
    using difference_type = std::ptrdiff_t;
    using value_type = dir_entry;
    using pointer = const dir_entry*;
    using reference = const dir_entry&;
    using iterator_category = std::input_iterator_tag;

    explicit dir_iter(dir_handle* hdl_ref = nullptr) noexcept;

    dir_iter& operator++();
    chatload::deref_proxy<value_type> operator++(int);

    reference operator*() noexcept;
    pointer operator->() noexcept;
};
}  // namespace os
}  // namespace chatload


#endif  // CHATLOAD_OS_H
