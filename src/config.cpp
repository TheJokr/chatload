/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2017  Leo Blöcher
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
#include "config.hpp"

// Streams
#include <iostream>
#include <fstream>
#include <sstream>

// Containers
#include <string>
#include <vector>
#include <iterator>

// Exceptions
#include <exception>

// C++ REST SDK (JSON)
#include <cpprest/json.h>

// chatload components
#include "constants.hpp"


namespace {
// Returns true if filename exists or false otherwise
inline bool fileExists(const std::wstring& filename) {
    return std::wifstream(filename).good();
}


// Returns a std::wstring with the content of filename or an empty string if it fails
std::wstring get_file_content(const std::wstring& filename) {
    std::wifstream in(filename);
    std::wstring content;

    if (in.good()) {
        in.seekg(0, std::wifstream::end);
        content.resize(in.tellg());

        in.seekg(0, std::wifstream::beg);
        in.read(&content[0], content.size());
    } else {
        std::cerr << "ERROR: Can't read from file" << std::endl;
    }

    return content;
}


// Returns true after writing content to filename or false if it fails
bool set_file_content(const std::wstring& filename, const std::wstring& content) {
    std::wofstream out(filename, std::wofstream::trunc);

    if (out.good()) {
        out << content;
        return true;
    } else {
        std::cerr << "ERROR: Can't write to file" << std::endl;
    }

    return false;
}


// Returns a std::vector with parts of input splitted at delim
std::vector<std::wstring> splitString(const std::wstring& input, const std::wstring::value_type& delim) {
    std::wstringstream wss(input);
    std::wstring content;
    std::vector<std::wstring> parts;

    while (std::getline(wss, content, delim)) {
        parts.push_back(content);
    }

    return parts;
}


// Returns a std::wstring with pretty output from input (serialized JSON document)
std::wstring prettyJSON(std::wstring input) {
    bool isString = false;
    for (std::wstring::size_type iter = 0; iter < input.length(); iter++) {
        // Beginning/end of string within JSON document
        if (input[iter] == L'"' && (iter == 0 || input[iter - 1] != L'\\')) {
            isString = !isString;
        }
        if (isString) {
            continue;
        }

        // EOL after opening/closing brackets and commas
        // Space after colons
        if ((input[iter] == L'[' && input[iter + 1] != L']') ||
            (input[iter] == L'{' && input[iter + 1] != L'}') ||
            input[iter] == L',')
        {
            input.insert(++iter, L"\n");
        }
        else if ((input[iter] == L']' && input[iter - 1] != L'[') ||
                 (input[iter] == L'}' && input[iter - 1] != L'{'))
        {
            input.insert(iter++, L"\n");
        }
        else if (input[iter] == L':')
        {
            input.insert(++iter, L" ");
        }
    }

    // Indentation (2 spaces)
    isString = false;
    unsigned int indentLevel = 0;
    for (std::wstring::size_type iter = 0; iter < input.length(); iter++) {
        // Beginning/end of string within JSON document
        if (input[iter] == L'"' && (iter == 0 || input[iter - 1] != L'\\')) {
            isString = !isString;
        }
        if (isString) {
            continue;
        }

        // Add/remove indent level after opening/closing brackets
        if (input[iter] == L'[' || input[iter] == L'{') {
            indentLevel += 1;
        }
        if (input[iter + 1] == L']' || input[iter + 1] == L'}') {
            indentLevel -= (indentLevel == 0 ? 0 : 1);
        }

        if (input[iter] == L'\n') {
            for (unsigned int indentIter = 0; indentIter < indentLevel; indentIter++) {
                input.insert(iter + 1, L"  ");
                iter += 2;
            }
        }
    }

    return input;
}
}  // Anonymous namespace


namespace chatload {
// chatload::config load/save functions
// Constructor
config::config(const std::wstring& filename) {
    load(filename);
}

// Returns true after loading filename as new configuration or false if the default configuration is used
bool config::load(const std::wstring& filename) {
    this->storage_path = filename;

    if (fileExists(this->storage_path)) {
        try {
            this->storage = web::json::value::parse(get_file_content(this->storage_path).c_str());
            return true;
        } catch (std::exception& ex) {
            std::cerr << "ERROR: " << ex.what() << std::endl;
            std::cout << "Loading default configuration" << std::endl;
            this->storage = web::json::value::parse(chatload::DEFAULTCONFIG);
        }
    } else {
        this->storage = web::json::value::parse(chatload::DEFAULTCONFIG);
        save();
    }
    return false;
}

// Returns true after saving the current configuration to storagePath or false if it fails
bool config::save() {
    try {
        set_file_content(this->storage_path, prettyJSON(this->storage.serialize()));
        return true;
    } catch (std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
    }
    return false;
}

// Returns true after reloading the configuration from storagePath or false if it fails
bool config::reload() {
    if (fileExists(this->storage_path)) {
        try {
            this->storage = web::json::value::parse(get_file_content(this->storage_path).c_str());
            return true;
        } catch (std::exception& ex) {
            std::cerr << "ERROR: " << ex.what() << std::endl;
        }
    }
    return false;
}


// chatload::config get/set functions
// path is a std::wstring in the format "path/to/value" and specifies which value to get/set
// Returns a web::json::value with the content of path or an empty value if the lookup fails
web::json::value config::get(const std::wstring& path) {
    std::vector<std::wstring> pathParts = splitString(path, L'/');
    web::json::value val;

    try {
        val = this->storage.at(pathParts[0]);
        for (auto iter = std::next(pathParts.begin()); iter != pathParts.end(); iter++) {
            val = val.at(*iter);
        }
    } catch (web::json::json_exception& ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
    }

    return val;
}

// Returns true after setting content at path or false if it fails
bool config::set(const std::wstring& path, web::json::value content) {
    std::vector<std::wstring> pathParts = splitString(path, L'/');

    try {
        web::json::value& val = this->storage[pathParts[0]];
        for (auto iter = std::next(pathParts.begin()); iter != pathParts.end(); iter++) {
            val = val[*iter];
        }

        val = content;
        return true;
    } catch (web::json::json_exception& ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
    }

    return false;
}
}  // namespace chatload
