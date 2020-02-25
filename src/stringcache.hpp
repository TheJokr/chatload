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
#ifndef CHATLOAD_STRINGCACHE_H
#define CHATLOAD_STRINGCACHE_H


// C headers
#include <cstdint>
#include <climits>

// Containers
#include <vector>

// Utility
#include <type_traits>

// xxHash3
#include <xxh3.h>

namespace chatload {
// Inspired by https://cs.stackexchange.com/a/24122
template<std::size_t index_bits, typename T = std::uint_least32_t>
class string_cache {
private:
    static_assert(std::is_unsigned<T>::value, "Value (T) must be an unsigned arithmetic type");
    static_assert((index_bits + sizeof(T) * CHAR_BIT) <= (sizeof(XXH64_hash_t) * CHAR_BIT),
                  "Index and value (T) may not exceed the size of XXH64_hash_t");
    std::vector<T> cache;

public:
    // Default initialization (all 0s) should be fine, since a collision is unlikely
    string_cache() : cache(1ull << index_bits) {};

    // Returns whether the key was added or not, i.e., whether the key is new or not
    template<typename Sequence>
    bool add_if_absent(const Sequence& key) noexcept {
        constexpr std::size_t full_mask = (1ull << (index_bits + sizeof(T) * CHAR_BIT)) - 1;
        constexpr std::size_t idx_mask = (1ull << index_bits) - 1;
        constexpr std::size_t value_mask = full_mask ^ idx_mask;

        // xxHash takes size in bytes
        XXH64_hash_t hash = XXH3_64bits(key.data(), sizeof(typename Sequence::value_type) * key.size());
        std::size_t idx = hash & idx_mask;
        T val = (hash & value_mask) >> index_bits;

        if (this->cache[idx] == val) {
            return false;
        } else {
            this->cache[idx] = val;
            return true;
        }
    };
};
}  // namespace chatload


#endif  // CHATLOAD_STRINGCACHE_H
