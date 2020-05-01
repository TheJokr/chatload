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
#ifndef CHATLOAD_LOGPARSER_H
#define CHATLOAD_LOGPARSER_H


// C headers
#include <cstdint>
#include <ctime>

// Containers
#include <vector>
#include <bitset>

// Utility
#include <type_traits>

// Boost
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>

// xxHash3
#include <xxh3.h>

// robin_hood
#include <robin_hood.h>

namespace chatload {
template<typename T, typename U>
struct integral_fits_into : std::integral_constant<bool,
    std::is_integral<T>::value && std::is_integral<U>::value &&
    std::is_signed<T>::value == std::is_signed<U>::value &&
    sizeof(U) >= sizeof(T)
> {};


template<typename Sequence>
struct seq_xxh3 {
    static_assert(integral_fits_into<XXH64_hash_t, std::size_t>::value, "XXH64_hash_t must fit into size_t");
    std::size_t operator()(const Sequence& seq) const noexcept {
        return ::XXH3_64bits(seq.data(), sizeof(typename Sequence::value_type) * seq.size());
    }
};


class logparser {
private:
    // Really big chat channels may reach thousands of users,
    // but not all of those actually write messages, i.e., show up in the logs.
    static constexpr std::size_t PRE_ALLOC_NAMES = 1024;

    // Constant to calculate pre-allocation size for output buffer
    // Average name length is ~12 characters, increase it a little for outliers
    static constexpr std::size_t HIGH_NAME_LEN = 16;

    // Separators used in the chatload protocol
    // See https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Field_separators
    static constexpr unsigned char RECORD_SEP = 0x1E;  // between elements of a report
    static constexpr unsigned char GROUP_SEP = 0x1D;  // between reports

    // There is no std::ctype<char16_t> facet on macOS, so we need to roll our own
    // character classification. Character names are required to be ASCII by CCP.
    static constexpr std::size_t ASCII_RANGE = (1 << 7);
    static const std::bitset<ASCII_RANGE> REGULAR_CHARS;

    struct char_entry {
        using time_type = std::int64_t;
        using count_type = std::uint64_t;

        // Members are stored in the chatload protocol format, so we
        // need to ensure time_t is actually a 64bit signed integer.
        static_assert(integral_fits_into<std::time_t, time_type>::value,
                      "time_t must fit into a 64 bit signed integer");

        time_type first_msg;
        time_type last_msg;
        count_type msg_count;

        explicit char_entry(time_type initial_msg) noexcept :
                first_msg(initial_msg), last_msg(initial_msg), msg_count(1) {}

        void update(time_type latest_msg) noexcept {
            this->last_msg = latest_msg;
            ++this->msg_count;
        }
    };

    robin_hood::unordered_map<boost::u16string_view, char_entry, seq_xxh3<boost::u16string_view>> char_map;

public:
    struct parser_res {
        std::uint_least64_t report_count = 0;
        std::vector<unsigned char> buffer;
    };

    explicit logparser() {
        this->char_map.reserve(PRE_ALLOC_NAMES);
    };

    // Caller must ensure that underlying string doesn't move in memory
    parser_res operator()(boost::u16string_view log);

private:
    static constexpr bool is_special_char(char16_t c) noexcept {
        return c == u'-' || c == u'\'';
    }

    static constexpr bool is_space_char(char16_t c) noexcept {
        return c == u' ';
    }

    static bool is_allowed_char(char16_t c) noexcept {
        constexpr std::size_t ascii_mask = ASCII_RANGE - 1;
        return REGULAR_CHARS[c & ascii_mask] || is_special_char(c);
    }

    // Extracts the channel name and strips the preceding part of the header from log
    // NOLINTNEXTLINE(bugprone-exception-escape): manual bounds check is present
    static boost::u16string_view parse_head(boost::u16string_view& log) noexcept;

    // Returns whether there is data left to parse in log
    static bool strip_remainder(boost::u16string_view& log, bool force_progress = false) noexcept;

    // Extracts the time header preceding each message and strips it from log
    static boost::optional<char_entry::time_type> parse_time(boost::u16string_view& log) noexcept;

    // Extracts the message's sender name and strips it from log
    // This parser adheres strictly to CCP's naming policy and the log format
    // NOLINTNEXTLINE(bugprone-exception-escape): manual bounds check is present
    static boost::u16string_view parse_name(boost::u16string_view& log) noexcept;
};
}  // namespace chatload


#endif  // CHATLOAD_LOGPARSER_H
