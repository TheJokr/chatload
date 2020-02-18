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

// Darwin API configuration
#define _DARWIN_USE_64_BIT_INODE

// Forward declaration
#include "os.hpp"

// C headers
#include <cerrno>
#include <cstdint>

// Containers
#include <string>
#include <array>

// Exceptions
#include <stdexcept>

// Utility
#include <memory>
#include <utility>
#include <type_traits>

// Darwin API
#include <limits.h>  // NOLINT(modernize-deprecated-headers): stick to OSX docs
#include <sysdir.h>
#include <wordexp.h>
#include <dirent.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

// OpenSSL
#include <openssl/ssl.h>

// chatload components
#include "common.hpp"


namespace {
template<typename T>
using cf_unique_ref = std::unique_ptr<typename std::remove_pointer<T>::type, decltype(&CFRelease)>;

void addAllCertsToStore(cf_unique_ref<CFArrayRef> certs_array, X509_STORE* store) noexcept {
    if (!certs_array || !store) { return; }

    CFArrayApplyFunction(certs_array.get(), {0, CFArrayGetCount(certs_array.get())}, [](CFTypeRef cert_ref, void* store) {
        CFDataRef cert_export;
        if (SecItemExport(cert_ref, kSecFormatOpenSSL, 0, nullptr, &cert_export) != errSecSuccess) {
            // Silently skip current certificate
            return;
        }

        const unsigned char* cert_raw = CFDataGetBytePtr(cert_export);
        X509* cert = d2i_X509(nullptr, &cert_raw, CFDataGetLength(cert_export));
        if (!cert) {
            // Silently skip current certificate
            CFRelease(cert_export);
            return;
        }

        X509_STORE_add_cert(static_cast<X509_STORE*>(store), cert);
        CFRelease(cert_export);
        OPENSSL_free(cert);
    }, store);
}

cf_unique_ref<CFArrayRef> getRootsFromSearchList() noexcept {
    // SecItemCopyMatching uses keychain search list by default
    constexpr std::size_t query_entries = 5;
    static std::array<CFTypeRef, query_entries> keys = { kSecClass, kSecMatchLimit, kSecMatchTrustedOnly,
                                                         kSecMatchValidOnDate, kSecReturnRef };
    static std::array<CFTypeRef, query_entries> values = { kSecClassCertificate, kSecMatchLimitAll, kCFBooleanTrue,
                                                           kCFNull, kCFBooleanTrue };
    cf_unique_ref<CFDictionaryRef> query = {
            CFDictionaryCreate(nullptr, keys.data(), values.data(), query_entries, nullptr, nullptr), &CFRelease
    };

    // SecItemCopyMatching returns CFArray because of kSecMatchLimitAll
    CFArrayRef tmp;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (!query || SecItemCopyMatching(query.get(), reinterpret_cast<CFTypeRef*>(&tmp)) != errSecSuccess) {
        // Silently ignore errors while retrieving certificates
        tmp = nullptr;
    }

    return { tmp, &CFRelease };
}

cf_unique_ref<CFArrayRef> getRootsFromSystem() noexcept {
    CFArrayRef tmp;
    if (SecTrustCopyAnchorCertificates(&tmp) != errSecSuccess) {
        // Silently ignore errors while retrieving certificates
        tmp = nullptr;
    }

    return { tmp, &CFRelease };
}
}  // Anonymous namespace


void chatload::os::loadTrustedCerts(SSL_CTX* ctx) noexcept {
    if (!ctx) { return; }

    X509_STORE* verify_store = SSL_CTX_get_cert_store(ctx);
    if (!verify_store) { return; }

    addAllCertsToStore(getRootsFromSystem(), verify_store);
    addAllCertsToStore(getRootsFromSearchList(), verify_store);
}


chatload::string chatload::os::getLogFolder() {
    std::string path;
    std::array<char, PATH_MAX> darres = {};
    wordexp_t we;

    auto state = sysdir_start_search_path_enumeration(SYSDIR_DIRECTORY_DOCUMENT, SYSDIR_DOMAIN_MASK_USER);
    while ((state = sysdir_get_next_search_path_enumeration(state, darres.data())) != 0) {
        if (wordexp(darres.data(), &we, WRDE_NOCMD) == 0) {
            if (we.we_wordc > 0) {
                path = we.we_wordv[0];
                wordfree(&we);
                path += "/EVE/logs/Chatlogs";
                break;
            } else {
                wordfree(&we);
            }
        }
    }

    return path;
}


namespace {
struct dtype_validator {
    bool enable_dirs : 1, enable_hidden : 1, enable_system : 1;

    bool operator()(const struct dirent* d_entry) {
        if (!d_entry) { return false; }
        switch (d_entry->d_type) {
            case DT_REG:
                break;
            case DT_DIR:
                if (this->enable_dirs) { break; }
                return false;
            // Anything but files and directories (symlinks, sockets, pipes, devices, ...)
            default:
                if (this->enable_system) { break; }
                return false;
        }

        // Inspect name for leading dot (hidden entry; includes "." and "..")
        // Directory entries always have at least 1 character
        return (this->enable_hidden || d_entry->d_name[0] != '.');
    }
};

chatload::os::dir_entry dir_entry_from_dirent_stat(const struct dirent* f_entry, const struct stat* f_stat) {
    if (!f_entry || !f_stat) { return {}; }
    return { std::string(f_entry->d_name, f_entry->d_namlen), static_cast<std::uint_least64_t>(f_stat->st_size),
             static_cast<std::uint_least64_t>(f_stat->st_mtimespec.tv_sec) };
}
}  // Anonymous namespace


struct chatload::os::dir_handle::iter_state {
    dtype_validator valid_entry;
    DIR* dirp;
    const struct dirent* entry;
    struct stat entry_stat;
};


chatload::os::dir_handle::dir_handle() noexcept = default;

chatload::os::dir_handle::dir_handle(const chatload::string& dir, bool enable_dirs,
                                     bool enable_hidden, bool enable_system) :
        status(ACTIVE), state(new iter_state{ { enable_dirs, enable_hidden, enable_system }, {}, {}, {} }) {
    this->state->dirp = opendir(dir.c_str());

    if (!this->state->dirp) {
        throw std::runtime_error("Error opening dir_handle (Code " + std::to_string(errno) + ")");
    }

    this->fetch_next();
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

    closedir(this->state->dirp);
    this->status = CLOSED;
}

bool chatload::os::dir_handle::fetch_next() {
    if (this->status != ACTIVE) { return false; }

    // Reset errno to 0 to distinguish between EOD and error
    errno = 0;
    while ((this->state->entry = readdir(this->state->dirp)) && !this->state->valid_entry(this->state->entry)) {}

    if (!this->state->entry) {
        if (errno) {
            throw std::runtime_error("Error retrieving next file in dir_handle (Code " + std::to_string(errno) + ")");
        }

        this->status = EXHAUSTED;
        return false;
    }

    if (fstatat(dirfd(this->state->dirp), this->state->entry->d_name, &this->state->entry_stat, 0) == -1) {
        throw std::runtime_error("Error stating next file in dir_handle (Code " + std::to_string(errno) + ")");
    }

    this->cur_entry = dir_entry_from_dirent_stat(this->state->entry, &this->state->entry_stat);
    return true;
}
