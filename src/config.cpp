/*
 * chatload: collect EVE Online character names from chat logs
 * Copyright (C) 2015  Leo Blöcher
 *
 * This file is part of chatload.
 *
 * chatload is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * chatload is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with chatload.  If not, see <http://www.gnu.org/licenses/>.
 */


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

// Utility
#include <algorithm>

// Casablanca (JSON)
#include <cpprest\json.h>

// chatload::config forward declaration
#include "config.hpp"


// Returns true if filename exists, false otherwise
inline bool fileExists(const std::wstring& filename) {
    std::wifstream in(filename);

    if (in.good()) {
        in.close();
        return true;
    } else {
        in.close();
        return false;
    }
}


// Returns a std::wstring with the content of filename
std::wstring get_file_content(const std::wstring& filename) {
    std::wifstream in(filename);

    if (in.good()) {
        std::wstring content;

        in.seekg(0, std::wifstream::end);
        content.resize(in.tellg());

        in.seekg(0, std::wifstream::beg);
        in.read(&content[0], content.size());

        in.close();
        return content;
    } else {
        in.close();
        std::cout << "ERROR: Can't read file content" << std::endl;
        return L"";
    }
}


// Returns true after writing content to filename, false otherwise
bool set_file_content(const std::wstring& filename, const std::wstring& content) {
    std::wofstream out(filename, std::wofstream::out | std::wofstream::trunc);

    if (out.good()) {
        out << content;

        out.close();
        return true;
    } else {
        out.close();
        return false;
    }
}


// Returns a std::vector with parts of input splitted at delim
std::vector<std::wstring> splitString(const std::wstring& input, const wchar_t& delim) {
    std::wstringstream wss(input);
    std::wstring content;
    std::vector<std::wstring> parts;

    while (std::getline(wss, content, delim)) {
        parts.push_back(content);
    }

    return parts;
}


// Returns a std::wstring with the content of input without removeChar without modifying input
std::wstring removeCharacter(std::wstring input, const wchar_t& removeChar) {
    input.erase(std::remove(input.begin(), input.end(), removeChar), input.end());
    return input;
}


namespace chatload {
    // Default config
    static const web::json::value DEFAULTCONFIG = web::json::value::parse(L"{\"POST\": {\"host\": \"http://api.dashsec.com\", \"resource\": \"/charDump.php\", \"parameter\": \"name\"}}");

    // Chatload::config member functions
    // Constructor
    config::config(const std::wstring& filename) {
        load(filename);
    }

    // Returns true after loading filename as new configuration or false when the default configuration is used
    bool config::load(const std::wstring& filename) {
        cfgFilename = filename;

        if (fileExists(cfgFilename)) {
            try {
                cfgObj = web::json::value::parse(removeCharacter(get_file_content(cfgFilename), '\n'));
                return true;
            } catch (std::exception& ex) {
                std::cout << "ERROR: " << ex.what() << std::endl;
                cfgObj = chatload::DEFAULTCONFIG;
            }
        } else {
            cfgObj = chatload::DEFAULTCONFIG;
            save();
        }
        return false;
    }

    // Returns true after saving the current configuration to cfgFilename
    bool config::save() {
        try {
            set_file_content(cfgFilename, cfgObj.serialize());
        } catch (std::exception& ex) {
            std::cout << "ERROR: " << ex.what() << std::endl;
            return false;
        }
        return true;
    }

    // Returns true after reloading the configuration from cfgFilename or false when the default configuration is used
    bool config::reload() {
        if (fileExists(cfgFilename)) {
            try {
                cfgObj = web::json::value::parse(removeCharacter(get_file_content(cfgFilename), '\n'));
            } catch (std::exception& ex) {
                std::cout << "ERROR: " << ex.what() << std::endl;
            }
        } else {
            save();
            return false;
        }

        return true;
    }

    // Returns a web::json::value with the content of path or an empty value if the lookup fails
    web::json::value config::get(const std::wstring& path) {
        try {
            std::vector<std::wstring> objPath = splitString(path, L'/');

            web::json::value& val = cfgObj.at(objPath[0]);
            for (auto iter = std::next(objPath.begin()); iter != objPath.end(); iter++) {
                val = val.at(*iter);
            }

            return val;
        } catch (web::json::json_exception& ex) {
            std::cout << "ERROR: " << ex.what() << std::endl;
            return web::json::value();
        }
    }

    // Returns true after setting path to content or false if it fails
    bool config::set(const std::wstring& path, const std::wstring& content) {
        try {
            std::vector<std::wstring> objPath = splitString(path, L'/');

            web::json::value& val = cfgObj[objPath[0]];
            for (auto iter = std::next(objPath.begin()); iter != objPath.end(); iter++) {
                val = val[*iter];
            }

            val = web::json::value::string(content);
            return true;
        } catch (web::json::json_exception& ex) {
            std::cout << "ERROR: " << ex.what() << std::endl;
            return false;
        }
    }
}
