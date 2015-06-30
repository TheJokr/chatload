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


// Dynamic memory allocation
#include <new>

// Streams
#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>

// Containers
#include <string>
#include <vector>
#include <unordered_set>
#include <tuple>
#include <iterator>

// Threading
#include <future>

// Utility
#include <regex>
#include <chrono>

// Casablanca (HTTP Client, JSON)
#include <cpprest\http_client.h>
#include <cpprest\json.h>

// Configuration
#include "config.hpp"

// WinAPI
#include <Windows.h>
#include <ShlObj.h>


// Chatload constants
namespace chatload {
// Version
static const std::string VERSION = "1.3.0";
}


// Returns a std::wstring with current user's home folder
std::wstring GetHomeDirectory() {
    LPWSTR path[MAX_PATH];

    SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, path);
    return std::wstring(*path);
}


// Returns a std::vector<std::wstring> with all lines from all chat logs whose name matches pattern
std::vector<std::wstring> ReadLogs(bool showReadFiles, std::wregex& pattern) {
    std::wstring logDir = GetHomeDirectory() + L"\\Documents\\EVE\\logs\\Chatlogs\\";
    std::wstring filename;
    std::wstring line;
    std::vector<std::wstring> lines;

    WIN32_FIND_DATA data;
    HANDLE dHandle;

    // Ignore '.' and '..' (standard files)
    dHandle = FindFirstFile((logDir + L"*").c_str(), &data);
    FindNextFile(dHandle, &data);

    while (FindNextFile(dHandle, &data) != 0) {
        filename = data.cFileName;

        if (std::regex_match(filename, pattern)) {
            // EVE Online logs are UCS-2 (LE) encoded
            std::wifstream filestream(logDir + filename, std::ios::binary);
            filestream.imbue(std::locale(
                filestream.getloc(), new std::codecvt_utf16<wchar_t, 0xffff, std::consume_header>));

            // Ignore first 12 lines (metadata)
            for (int iter = 0; iter < 12; iter++) {
                std::getline(filestream, line);
            }

            while (std::getline(filestream, line)) {
                lines.push_back(line);
            }

            filestream.close();
            if (showReadFiles) {
                std::wcout << filename << std::endl;
            }
        }
    }

    FindClose(dHandle);
    return lines;
}


// Returns a std::vector<std::wstring> with all character names
std::vector<std::wstring> filterNames(const std::vector<std::wstring>& vec) {
    std::unordered_set<std::wstring> charNames;

    /*
     * Matches character names using wexp
     * Format: [ YYYY.MM.DD H:m:s ] CHARACTER_NAME > TEXT
     * wmatches[0] = full string
     * wmatches[1] = CHARACTER_NAME
     */
    std::wsmatch wmatches;
    std::wregex charNameRegex(L"\\[[\\d\\.\\s:]{21}\\]\\s([\\w\\s]+)\\s>.*");

    for (const auto& line : vec) {
        if (std::regex_search(line, wmatches, charNameRegex)) {
            charNames.insert(wmatches[1]);
        }
    }

    return std::vector<std::wstring>(charNames.begin(), charNames.end());
}


// Returns a std::wstring with the concatenation of vec using sep as seperator
std::wstring joinVec(const std::vector<std::wstring>& vec, const std::wstring& sep) {
    std::wstringstream wss;
    wss << vec[0];

    for (auto iter = std::next(vec.begin()); iter != vec.end(); iter++) {
        wss << sep;
        wss << *iter;
    }

    return wss.str();
}


int main(int argc, char* argv[]) {
    // Window title
    SetConsoleTitle(L"Chatload");


    // Version output
    if (argc == 2 && (std::strcmp(argv[1], "-V") == 0 || std::strcmp(argv[1], "--version") == 0)) {
        std::cout << argv[0] << " " << chatload::VERSION << std::endl;
        std::cout << "Copyright (C) 2015  Leo Bloecher" << std::endl << std::endl;
        std::cout << "This program comes with ABSOLUTELY NO WARRANTY." << std::endl;
        std::cout << "This is free software, and you are welcome to redistribute it under certain conditions.";
        std::cout << std::endl;
        return 0;
    }


    // Load configuration
    chatload::config cfg(L"config.json");


    std::cout << "This app scrapes your EVE Online chat logs for character names and adds them to a database";
    std::cout << std::endl << std::endl;


    // Read all logs
    std::cout << "Files read:" << std::endl;
    std::vector<std::wstring> allLines;
    try {
        allLines = ReadLogs(true, std::wregex(cfg.get(L"regex").as_string()));
    } catch (web::json::json_exception& ex) {
        std::cout << "ERROR: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << "Total of " << allLines.size() << " lines read (excluding metadata)" << std::endl << std::endl;


    // Extract character names asynchronously
    std::cout << "Filtering character names" << std::endl << std::endl;
    std::future<std::vector<std::wstring>> charNameThread = std::async(&filterNames, std::ref(allLines));


    // Create Casablanca client(s)
    // httpClients = std::vector<std::tuple<CLIENT, HOST, RESOURCE, PARAMETER>>
    std::cout << "Establishing connection(s)...";
    std::vector<std::tuple<web::http::client::http_client, std::wstring, std::wstring, std::wstring>> httpClients;
    try {
        web::json::array POSTEndpoints = cfg.get(L"POST").as_array();
        for (auto endpoint : POSTEndpoints) {
            httpClients.push_back(std::make_tuple(
                web::http::client::http_client(endpoint.at(L"host").as_string()),
                endpoint.at(L"host").as_string(),
                endpoint.at(L"resource").as_string(),
                endpoint.at(L"parameter").as_string()));
        }
    } catch (web::json::json_exception& ex) {
        std::cout << "ERROR: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << " " << httpClients.size() << " connection(s) established" << std::endl << std::endl;


    // Wait for character names to become available
    std::cout << "Waiting for character names filter. This might take some time...";
    std::future_status charNameStatus = charNameThread.wait_for(std::chrono::seconds(0));
    while (charNameStatus != std::future_status::ready) {
        charNameStatus = charNameThread.wait_for(std::chrono::seconds(1));
        std::cout << ".";
    }
    std::cout << std::endl << std::endl;

    // Retrieve character names
    std::vector<std::wstring> charNames = charNameThread.get();


    // POST character names
    std::cout << "Adding character names to database" << std::endl;
    for (auto client : httpClients) {
        pplx::task<web::http::http_response> postThread = std::get<0>(client).request(
            web::http::methods::POST,
            std::get<2>(client),
            std::get<3>(client) + L"=" + joinVec(charNames, L","),
            L"application/x-www-form-urlencoded");

        postThread.then([=](web::http::http_response response) {
            if (response.status_code() == web::http::status_codes::OK) {
                std::wcout << L"Successfully added character names to DB at " << std::get<1>(client) << std::endl;
            } else {
                std::wcout << L"Failed to add character names to DB at " << std::get<1>(client) << std::endl;
            }
        });
        postThread.wait();
    }
    std::cout << std::endl;


    // End program
    return 0;
}
