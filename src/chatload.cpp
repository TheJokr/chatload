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
#include <iterator>

// Threading
#include <thread>
#include <future>

// Utility
#include <cctype>
#include <regex>
#include <chrono>
#include <utility>

// Casablanca
#include <cpprest\http_client.h>

// WinAPI
#include <Windows.h>
#include <Lmcons.h>


// Chatload constants
namespace chatload {
    // Version
    static const std::string VERSION = "1.1.0";


    // POST Endpoint
    // CHATLOAD_HOST = Base URI with protocol, hostname, and optionally a port number (eg. http://example.com)
    static const std::wstring HOST = L"YOUR_HOST";
    // CHATLOAD_RESOURCE = Path, Query, and Fragment of POST endpoint (eg. /exampleScript.php)
    static const std::wstring RESOURCE = L"YOUR_RESOURCE";
}

// Returns a std::wstring with current user's name
std::wstring GetUserName() {
    wchar_t username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;

    GetUserName(username, &username_len);
    return std::wstring(username);
}


// Returns a std::vector<std::wstring> with all lines from all chat logs whose name matches pattern
std::vector<std::wstring> ReadLogs(bool showReadFiles = true, std::wregex pattern = std::wregex(L".*")) {
    std::wstring logDir = L"C:\\Users\\" + GetUserName() + L"\\Documents\\EVE\\logs\\Chatlogs\\";
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
            std::wifstream filestream(logDir + filename, std::ios::binary);
            // EVE Logs are UCS-2 (LE) encoded
            filestream.imbue(std::locale(filestream.getloc(), new std::codecvt_utf16<wchar_t, 0xffff, std::consume_header>));

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
    return lines;
}


// Returns a std::vector<std::wstring> with all character names
std::vector<std::wstring> extractNames(const std::vector<std::wstring>& vec) {
    std::unordered_set<std::wstring> charNames;

    /*
     * Matches character names using wexp
     * Format: [ YYYY.MM.DD H:m:s ] CHARACTER_NAME > TEXT
     * wmatches[0] = full string
     * wmatches[1] = CHARACTER_NAME
     */
    std::wsmatch wmatches;
    std::wregex wexp(L"\\[[\\d\\.\\s:]{21}\\]\\s([\\w\\s]+)\\s>.*");

    for (const auto& wstr : vec) {
        if (std::regex_search(wstr, wmatches, wexp)) {
            charNames.insert(wmatches[1]);
        }
    }

    return std::vector<std::wstring>(charNames.begin(), charNames.end());
}


// Returns true after dumping vec to filename
bool dumpVec(const std::vector<std::wstring>& vec, const std::wstring filename) {
    // Converter (wstring to string)
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

    std::ofstream dump(filename);
    for (const auto& wstr : vec) {
        dump << converter.to_bytes(wstr) << std::endl;
    }

    return true;
}


// Returns a std::wstring with the concatenation of vec using sep as seperator
std::wstring joinVec(const std::vector<std::wstring>& vec, const std::wstring sep) {
    bool isFirst = true;
    std::wstringstream wss;

    for (const auto& wstr : vec) {
        if (!isFirst) {
            wss << sep;
        }
        wss << wstr;
        isFirst = false;
    }

    return wss.str();
}


int main(int argc, char* argv[]) {
    // Window title
    SetConsoleTitle(L"Chatload");


    // Version output
    if (argc == 2 && (std::strcmp(argv[1], "-v") == 0 || std::strcmp(argv[1], "--version") == 0)) {
        std::cout << argv[0] << " " << chatload::VERSION << std::endl;
        std::cout << "Copyright (C) 2015  Leo Bloecher" << std::endl << std::endl;
        std::cout << "This program comes with ABSOLUTELY NO WARRANTY." << std::endl;
        std::cout << "This is free software, and you are welcome to redistribute it under certain conditions." << std::endl;
        return 0;
    }


    std::cout << "This app scrapes your EVE Online chat logs for character names and adds them to a database" << std::endl << std::endl;


    // Read all logs
    std::cout << "Files read:" << std::endl;
    std::vector<std::wstring> allLines = ReadLogs();
    std::cout << "Total of " << allLines.size() << " lines read (excluding metadata)" << std::endl << std::endl;


    // Dump lines in the background (raw, debug only)
    #ifdef _DEBUG
    std::cout << "Dumping all read lines into linesDump.txt in the working directory" << std::endl << std::endl;
    std::thread dumpLinesThread(&dumpVec, std::ref(allLines), L"linesDump.txt");
    #endif


    // Extract character names asynchronously
    std::cout << "Filtering character names" << std::endl << std::endl;
    std::future<std::vector<std::wstring>> charNameThread = std::async(&extractNames, std::ref(allLines));


    // Create Casablanca client
    std::cout << "Establishing connection...";
    web::http::client::http_client client(chatload::HOST);
    std::cout << " " << "Connection established" << std::endl << std::endl;


    // Wait for character names to become available
    std::cout << "Waiting for character names. This might take some time...";
    std::future_status charNameStatus = charNameThread.wait_for(std::chrono::seconds(0));
    while (charNameStatus != std::future_status::ready) {
        charNameStatus = charNameThread.wait_for(std::chrono::seconds(1));
        std::cout << ".";
    }
    std::cout << std::endl;

    // Retrieve character names
    std::vector<std::wstring> charNames = charNameThread.get();
    std::cout << "Character names filtered" << std::endl << std::endl;


    // POST character names asynchronously
    std::cout << "Adding character names to DB" << std::endl;
    pplx::task<web::http::http_response> postChars = client.request(web::http::methods::POST, chatload::RESOURCE, L"name=" + joinVec(charNames, L","), L"application/x-www-form-urlencoded");
    postChars.then([=](web::http::http_response response) {
        if (response.status_code() == web::http::status_codes::OK) {
            std::cout << "Successfully added character names to DB" << std::endl << std::endl;
        } else {
            std::cout << "Failed to add character names to DB" << std::endl << std::endl;
        }
    });


    // Dump character names in the background (raw, debug only)
    #ifdef _DEBUG
    std::cout << "Dumping all character names into charDump.txt in the working directory" << std::endl << std::endl;
    std::thread dumpCharsThread(&dumpVec, std::ref(charNames), L"charDump.txt");
    #endif


    // Wait for dumps to finish (debug only)
    #ifdef _DEBUG
    dumpLinesThread.join();
    std::cout << "Chat logs dumped" << std::endl;

    dumpCharsThread.join();
    std::cout << "Character names dumped" << std::endl << std::endl;
    #endif


    // Wait for upload to finish
    postChars.wait();


    // End program
    std::cout << "Program has finished" << std::endl;

    return 0;
}