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
#ifndef CHATLOAD_EXCEPTION_H
#define CHATLOAD_EXCEPTION_H


// Exceptions
#include <exception>

// Utility
#include <utility>

// chatload components
#include "common.hpp"

namespace chatload {
inline namespace exception {
class runtime_error : public std::exception {
private:
    chatload::string msg;

public:
    explicit runtime_error(const chatload::string& what) : msg(what) {}
    explicit runtime_error(chatload::string&& what) noexcept : msg(std::move(what)) {}
    const char* what() const noexcept final { return "See what_cl"; }
    virtual chatload::string what_cl() const { return this->msg; }
};
}  // namespace exception
}  // namespace chatload


#endif  // CHATLOAD_EXCEPTION_H
