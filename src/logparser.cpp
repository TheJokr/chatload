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
#include "logparser.hpp"

// C headers
#include <cstdint>
#include <cassert>
#include <ctime>

// Containers
#include <array>
#include <bitset>

// Utility
#include <utility>
#include <type_traits>

// Boost
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/locale/encoding_utf.hpp>

// chatload components
#include "os.hpp"


namespace {
// Poor man's operator ""sv
template<typename charT, std::size_t len>
// NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
constexpr boost::basic_string_view<std::remove_cv_t<charT>> make_string_view(charT(&ptr)[len]) noexcept {
    static_assert(len >= 1, "String length must be at least 1 due to zero-termination");

    // Remove 1 from length to exclude zero terminator
    return { ptr, len - 1 };
}


// string_view conversion helper
namespace boost_conv = boost::locale::conv;

template<typename CharOut, typename CharIn>
auto conv_utf_to_utf(const boost::basic_string_view<CharIn>& str,
                     boost_conv::method_type how = boost_conv::default_method) {
    return boost_conv::utf_to_utf<CharOut>(str.data(), str.data() + str.size(), how);
}


// std::atoi for char16_t with static string length
template<std::size_t len>
int utf16_toint(boost::u16string_view::const_iterator iter) noexcept {
    int res = 0;
    for (std::size_t idx = 0; idx < len; ++idx) {
        res = res * 10 + (*(iter++) - u'0');
    }
    return res;
}

// Selfmade timestamp parser (existing ones all use iostreams)
// Assumes timestamp parameter is correctly sized (assert only)
boost::optional<std::tm> parse_log_timestamp(boost::u16string_view timestamp) noexcept {
    // Date format: YYYY.MM.DD HH:mm:ss
    static constexpr std::array<boost::u16string_view::size_type, 14> valid_idxs = {
        /* year */ 0, 1, 2, 3, /* month */ 5, 6, /* day */ 8, 9,
        /* hours */ 11, 12, /* minutes */ 14, 15, /* seconds */ 17, 18
    };
    assert(timestamp.size() >= valid_idxs.back() + 1);

    // Verify that we are actually dealing with numbers
    for (const auto idx : valid_idxs) {
        const auto c = timestamp[idx];
        if (c > u'9' || c < u'0') { return boost::none; }
    }

    // Per https://en.cppreference.com/w/cpp/chrono/c/tm
    std::tm res = {};
    res.tm_year = utf16_toint<4>(timestamp.begin()) - 1900;  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
    res.tm_mon = utf16_toint<2>(timestamp.begin() + 5) - 1;  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
    res.tm_mday = utf16_toint<2>(timestamp.begin() + 8);  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
    res.tm_hour = utf16_toint<2>(timestamp.begin() + 11);  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
    res.tm_min = utf16_toint<2>(timestamp.begin() + 14);  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
    res.tm_sec = utf16_toint<2>(timestamp.begin() + 17);  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment

    // Verify value ranges (utf16_toint doesn't return negative integers)
    if (res.tm_mon < 0 || res.tm_mon > 11 ||  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
            res.tm_mday < 1 || res.tm_mday > 31 ||  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
            res.tm_hour > 23 ||  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
            res.tm_min > 59 ||  // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): see comment
            res.tm_sec > 60) { return boost::none; }

    return res;
}


// Helpers to write binary representations to output buffer
template<typename T>
std::pair<const unsigned char*, const unsigned char*> get_byte_range(const T* ptr) noexcept {
    static_assert(std::is_trivially_copyable<T>::value && !std::is_array<T>::value,
                  "T must be a trivially-copyable non-array type");

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): cast to bytes
    const auto* byte_ptr = reinterpret_cast<const unsigned char*>(ptr);
    return { byte_ptr, byte_ptr + sizeof(T) };
}

template<typename Sequence>
std::pair<const unsigned char*, const unsigned char*> get_byte_range(const Sequence& seq) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): cast to bytes
    const auto* ptr = reinterpret_cast<const unsigned char*>(seq.data());
    return { ptr, ptr + seq.size() * sizeof(typename Sequence::value_type) };
}
}  // Anonymous namespace


// NOLINTNEXTLINE(cert-err58-cpp): IIFE is noexcept
const std::bitset<chatload::logparser::ASCII_RANGE> chatload::logparser::REGULAR_CHARS = []() noexcept {
    constexpr std::size_t ascii_mask = chatload::logparser::ASCII_RANGE - 1;

    // From https://community.eveonline.com/support/policies/naming-policy-en/
    std::bitset<chatload::logparser::ASCII_RANGE> res;
    for (char16_t c = u'A'; c <= u'Z'; ++c) { res[c & ascii_mask] = true; }  // Uppercase latin letters
    for (char16_t c = u'a'; c <= u'z'; ++c) { res[c & ascii_mask] = true; }  // Lowercase latin letters
    for (char16_t c = u'0'; c <= u'9'; ++c) { res[c & ascii_mask] = true; }  // Digits

    return res;
}();


// Necessary due to ODR-use of these constants in chatload::logparser::operator()
// std::vector<T>::push_back takes its arguments by reference
constexpr unsigned char chatload::logparser::RECORD_SEP;  // NOLINT(readability-redundant-declaration)
constexpr unsigned char chatload::logparser::GROUP_SEP;  // NOLINT(readability-redundant-declaration)


chatload::logparser::parser_res chatload::logparser::operator()(boost::u16string_view log) {
    // "EVE System" is not a real character name, we filter it here.
    // It is present in *every* chat log because it posts the MOTD.
    static constexpr auto eve_system_name = make_string_view(u"EVE System");
    parser_res res{};

    const auto channel_name = parse_head(log);
    if (channel_name.empty()) { return res; }

    this->char_map.clear();

    // Strips the remainder of the header on the first invocation
    // and the remaining message on subsequent invocations.
    // force_progress_after_err ensures progress after failure to parse a line.
    bool force_progress_after_err = false;
    while (strip_remainder(log, force_progress_after_err)) {
        const auto msg_time = parse_time(log);
        force_progress_after_err = !msg_time.has_value();
        if (force_progress_after_err) { continue; }

        const auto char_name = parse_name(log);
        force_progress_after_err = char_name.empty();
        if (force_progress_after_err || char_name == eve_system_name) { continue; }

        auto iter = this->char_map.find(char_name);
        if (iter == this->char_map.end()) {
            // Create new character entry (uses char_entry's single-arg constructor)
            this->char_map.emplace(char_name, msg_time.get());
        } else {
            iter->second.update(msg_time.get());
        }
    }

    if (this->char_map.empty()) { return res; }
    res.report_count = this->char_map.size();

    const auto channel_utf8 = conv_utf_to_utf<char>(channel_name);
    const auto channel_bytes = get_byte_range(channel_utf8);

    // Pre-allocate output buffer based on report size estimate
    const auto est_buf_size = [&] {
        auto bytes_per_entry = HIGH_NAME_LEN +  // Character name (+ RS)
            channel_utf8.size() * sizeof(channel_utf8.front()) +  // Channel name (+ RS)
            2 * sizeof(char_entry::time_type) + sizeof(char_entry::count_type) +  // char_entry members (+ GS)
            2 * sizeof(RECORD_SEP) + sizeof(GROUP_SEP);
        return bytes_per_entry * this->char_map.size();
    }();
    res.buffer.reserve(est_buf_size);

    for (const auto& chars : this->char_map) {
        const auto name_utf8 = conv_utf_to_utf<char>(chars.first);

        const auto name_bytes = get_byte_range(name_utf8);
        const auto first_ts_bytes = get_byte_range(&chars.second.first_msg);
        const auto last_ts_bytes = get_byte_range(&chars.second.last_msg);
        const auto msg_count_bytes = get_byte_range(&chars.second.msg_count);

        res.buffer.insert(res.buffer.end(), name_bytes.first, name_bytes.second);
        res.buffer.push_back(RECORD_SEP);

        res.buffer.insert(res.buffer.end(), channel_bytes.first, channel_bytes.second);
        res.buffer.push_back(RECORD_SEP);

        res.buffer.insert(res.buffer.end(), first_ts_bytes.first, first_ts_bytes.second);
        res.buffer.insert(res.buffer.end(), last_ts_bytes.first, last_ts_bytes.second);
        res.buffer.insert(res.buffer.end(), msg_count_bytes.first, msg_count_bytes.second);
        res.buffer.push_back(GROUP_SEP);
    }

    return res;
}


// NOLINTNEXTLINE(bugprone-exception-escape): manual bounds check is present
boost::u16string_view chatload::logparser::parse_head(boost::u16string_view& log) noexcept {
    static constexpr auto channel_head = make_string_view(u"Channel Name:");

    // First, find the channel name header. Then, starting from the first character following
    // that header, skip as many spaces as possible. This is where the channel name starts.
    // It is followed directly by a newline, so we use that as our end sentinel.
    const auto head_start = log.find(channel_head);
    if (head_start == boost::u16string_view::npos) { return {}; }
    const auto val_start = log.find_first_not_of(u' ', head_start + channel_head.size());
    if (val_start == boost::u16string_view::npos) { return {}; }
    const auto val_end = log.find(u'\n', val_start);
    if (val_end == boost::u16string_view::npos) { return {}; }

    // Return substring [val_start, val_end)
    auto res = log.substr(val_start, val_end - val_start);
    log.remove_prefix(val_end + 1);
    return res;
}

bool chatload::logparser::strip_remainder(boost::u16string_view& log, bool force_progress) noexcept {
    // Every log line starts with a date enclosed in brackets.
    // force_progress means we remove at least 1 character to advance the parsing.
    const auto next_line = log.find(u'[', static_cast<boost::u16string_view::size_type>(force_progress));
    if (next_line == boost::u16string_view::npos) { return false; }

    // next_line is a valid index in log, so next_line < log.length() holds.
    // Therefore, log can't be empty after this operation.
    log.remove_prefix(next_line);
    return true;
}

boost::optional<chatload::logparser::char_entry::time_type>
chatload::logparser::parse_time(boost::u16string_view& log) noexcept {
    // Message format: [ YYYY.MM.DD HH:mm:ss ] CHARACTER NAME > TEXT
    // Some lines may be damaged due to missing synchronization on CCP's part
    constexpr boost::u16string_view::size_type header_len = 24;
    constexpr boost::u16string_view::size_type timestamp_len = header_len - 5;

    // Header must be properly delimited
    if (log.length() <= header_len || log[header_len - 2] != u']') { return boost::none; }

    boost::optional<std::tm> timestamp = parse_log_timestamp(log.substr(2, timestamp_len));
    if (!timestamp) { return boost::none; }

    // EVE times are always UTC. -1 indicates an error in timegm.
    auto time_res = chatload::os::timegm(timestamp.get_ptr());
    if (time_res == -1) { return boost::none; }

    log.remove_prefix(header_len);
    return time_res;
}

// NOLINTNEXTLINE(bugprone-exception-escape): manual bounds check is present
boost::u16string_view chatload::logparser::parse_name(boost::u16string_view& log) noexcept {
    // See https://community.eveonline.com/support/policies/naming-policy-en/ for constraints
    constexpr boost::u16string_view::size_type min_len = 3;
    constexpr boost::u16string_view::size_type max_first_len = 24;
    constexpr boost::u16string_view::size_type max_family_len = 12;
    constexpr unsigned char max_num_space = 2;  // One in first name, one between first and family name

    // Add 1 for space between first name and family name
    // Add 2 for ' >' (end sentinel) lookahead
    const auto max_len = std::min(log.length(), max_first_len + max_family_len + 1 + 2);
    boost::u16string_view::size_type name_len = 0;
    boost::u16string_view::size_type real_first_len = 0;
    unsigned char num_space = 0;

    for (; name_len < max_len; ++name_len) {
        char16_t cur = log[name_len];

        if (!is_allowed_char(cur)) {
            // The character is not in the valid range; abort
            // OR The name is not terminated properly; abort
            // OR The next character is *also* a space; abort
            if (!is_space_char(cur) || name_len + 1 >= max_len || is_space_char(log[name_len + 1])) {
                return {};
            }

            // Name is complete; go on to verify it
            if (log[name_len + 1] == u'>') { break; }

            // Name exceeds max_num_space spaces and does not end here; abort
            // OR First name exceeds maximum length; abort
            if (num_space >= max_num_space || name_len > max_first_len) { return {}; }

            // Space is allowed to appear here; save first name length and increase counter
            // If this is the second space, the previous part was first + middle name.
            // Those are counted together for length constraints, so this assignment is ok in either case.
            real_first_len = name_len;
            ++num_space;
        }
    }

    // Calculate effective lengths of first and family name based on presence of spaces
    bool has_space = num_space > 0;
    const auto real_family_len = has_space ? name_len - real_first_len - 1 : 0;
    if (!has_space) { real_first_len = name_len; }

    // Verify additional properties:
    // - Minimum and maximum length constraints (first and family name)
    // - Name can't be as long as log because of ' >' (relevant if max_len == log.length())
    // - Name can't start or end with special characters or space
    //   - Can't end with space is enforced in loop already (no repeated spaces + followed by ' >')
    bool minmax_fail = name_len < min_len || real_first_len > max_first_len || real_family_len > max_family_len;
    if (minmax_fail || name_len >= log.length()) { return {}; }

    // This needs to come after the previous 2 conditions so that the indices are guaranteed to be in bounds
    char16_t first = log[0];
    char16_t last = log[name_len - 1];
    if (is_special_char(first) || is_space_char(first) || is_special_char(last)) { return {}; }

    auto res = log.substr(0, name_len);
    log.remove_prefix(name_len);
    return res;
}
