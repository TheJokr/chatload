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

// Forward declaration
#include "os.hpp"

// Containers
#include <string>

// Exceptions
#include <system_error>

// Utility
#include <utility>

// WinAPI
#include <Windows.h>
#include <objbase.h>
#include <ShlObj.h>
#include <wincrypt.h>

// Boost
#include <boost/optional.hpp>

// OpenSSL
#include <openssl/ssl.h>

// chatload components
#include "common.hpp"


void chatload::os::loadTrustedCerts(SSL_CTX* ctx) noexcept {
    if (!ctx) { return; }

    X509_STORE* verify_store = SSL_CTX_get_cert_store(ctx);
    if (!verify_store) { return; }

    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    constexpr DWORD flags = CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG | CERT_SYSTEM_STORE_CURRENT_USER;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
    HCERTSTORE system_store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL, flags, L"ROOT");
    if (!system_store) { return; }

    PCCERT_CONTEXT cert_export = NULL;  // NOLINT(modernize-use-nullptr)
    while ((cert_export = CertEnumCertificatesInStore(system_store, cert_export)) != nullptr) {
        const unsigned char* cert_raw = cert_export->pbCertEncoded;
        X509* cert = d2i_X509(NULL, &cert_raw, cert_export->cbCertEncoded);  // NOLINT(modernize-use-nullptr)
        if (cert) {
            X509_STORE_add_cert(verify_store, cert);
            OPENSSL_free(cert);
        }
    }

    CertCloseStore(system_store, 0);
}


chatload::string chatload::os::getLogFolder() {
    PWSTR winres;  // NOLINT(cppcoreguidelines-init-variables): initialized below
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &winres);  // NOLINT(modernize-use-nullptr)
    if (!SUCCEEDED(hr)) {
        throw std::system_error({ hr, std::system_category() }, "SHGetKnownFolderPath");
    }

    std::wstring path = winres;
    CoTaskMemFree(winres);
    path.append(LR"(\EVE\logs\Chatlogs)");
    return path;
}

boost::optional<chatload::string> chatload::os::getCacheFile() {
    PWSTR winres;  // NOLINT(cppcoreguidelines-init-variables): initialized below
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &winres);  // NOLINT(modernize-use-nullptr)
    if (!SUCCEEDED(hr)) {
        return boost::none;
    }

    std::wstring path = winres;
    CoTaskMemFree(winres);
    path.append(LR"(\chatload\filecache.tsv)");
    return path;
}


namespace {
chatload::os::dir_entry dir_entry_from_find_data(const WIN32_FIND_DATAW& data) {
    // NOLINTNEXTLINE(hicpp-signed-bitwise): false positive
    return { data.cFileName, (static_cast<DWORDLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow,
             // NOLINTNEXTLINE(hicpp-signed-bitwise): false positive
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
                                             // NOLINTNEXTLINE(modernize-use-nullptr)
                                             FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if (this->state->find_hdl == INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
        throw std::system_error({ static_cast<int>(GetLastError()), std::system_category() }, "FindFirstFileEx");
    }

    if (!enable_dirs) { this->state->file_attrs |= FILE_ATTRIBUTE_DIRECTORY; }  // NOLINT(hicpp-signed-bitwise)
    if (!enable_hidden) { this->state->file_attrs |= FILE_ATTRIBUTE_HIDDEN; }  // NOLINT(hicpp-signed-bitwise)
    if (!enable_system) { this->state->file_attrs |= FILE_ATTRIBUTE_SYSTEM; }  // NOLINT(hicpp-signed-bitwise)

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
    bool ok;  // NOLINT(cppcoreguidelines-init-variables): initialized below
    // return value is BOOL, true comparison is required by MSVC C4706
    // NOLINTNEXTLINE(readability-implicit-bool-conversion,readability-simplify-boolean-expr)
    while ((ok = FindNextFileW(this->state->find_hdl, &this->state->find_data)) == true &&
           (this->state->find_data.dwFileAttributes & this->state->file_attrs)) {}

    if (!ok) {
        DWORD ec = GetLastError();
        if (ec != ERROR_NO_MORE_FILES) {
            throw std::system_error({ static_cast<int>(ec), std::system_category() }, "FindNextFile");
        }

        this->status = EXHAUSTED;
    } else {
        this->cur_entry = dir_entry_from_find_data(this->state->find_data);
    }

    return ok;
}
