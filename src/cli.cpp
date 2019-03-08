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
#include "cli.hpp"

// C headers
#include <cstdlib>

// Streams
#include <iostream>
#include <fstream>
#include <sstream>

// Containers
#include <string>
#include <vector>

// Utility
#include <utility>

// Boost
#include <boost/optional.hpp>
#include <boost/program_options.hpp>

// chatload components
#include "constants.hpp"
#include "exception.hpp"


namespace po = boost::program_options;

namespace {
void parseConfig(const std::wstring& file, po::variables_map& vm) {
    std::wifstream in(file);
    if (in) {
        po::options_description cfg_options;
        cfg_options.add_options()
            ("network.host", po::wvalue<std::vector<std::wstring>>())
            ("regex", po::wvalue<std::wstring>()->default_value(L".*", ".*"));

        po::store(po::parse_config_file(in, cfg_options), vm);
    } else {
        vm.emplace(std::string("regex"), po::variable_value(std::wstring(L".*"), true));
    }
}

std::vector<chatload::cli::host> parseHosts(po::variables_map& vm) {
    std::vector<chatload::cli::host> hosts;

    if (vm.count("network.host")) {
        const auto& host_lits = vm["network.host"].as<std::vector<std::wstring>>();
        for (const auto& host_lit : host_lits) {
            auto name_end = host_lit.find(L':');
            if (name_end == std::wstring::npos) {
                hosts.push_back({ host_lit, chatload::DEFAULTPORT });
            } else {
                hosts.push_back({ host_lit.substr(0, name_end),
                                  static_cast<std::uint_least16_t>(std::stoi(host_lit.substr(name_end + 1))) });
            }
        }
    } else {
        hosts.push_back({ chatload::DEFAULTHOST, chatload::DEFAULTPORT });
    }

    return hosts;
}
}  // Anonymous namespace


chatload::cli::options chatload::cli::parseArgs(int argc, wchar_t* argv[]) {
    namespace clcfg = chatload::constants;

    po::options_description visible_options("Allowed options");
    visible_options.add_options()
        ("help,h", "display this help text and exit")
        ("version,V", "display version information and exit")
        ("verbose,v", "list read logs")
        ("force,f", "read all logs, even if they have been read before")
        ("config", po::wvalue<std::wstring>()->default_value(clcfg::CONFIGFILE, clcfg::CONFIG_HELP), "config file")
        ("cache", po::wvalue<std::wstring>()->default_value(clcfg::CACHEFILE, clcfg::CACHE_HELP), "cache file");

    boost::optional<std::wstring> log_path;
    po::options_description cli_options;
    cli_options.add(visible_options);
    cli_options.add_options()("log-path", po::wvalue(&log_path), "path to logs");

    po::positional_options_description pos_options;
    pos_options.add("log-path", 1);

    po::variables_map vm;
    po::store(po::wcommand_line_parser(argc, argv).options(cli_options).positional(pos_options).run(), vm);
    parseConfig(vm["config"].as<std::wstring>(), vm);
    po::notify(vm);

    bool ver = vm.count("version"), help = vm.count("help");
    if (ver) {
        std::wcout << argv[0] << " version " << chatload::VERSION << "\n"
                   << "Copyright (C) 2015-2019  Leo Bloecher\n"
                   << "This program comes with ABSOLUTELY NO WARRANTY.\n"
                   << "This is free software, and you are welcome to redistribute it under certain conditions."
                   << std::endl;
    }

    if (help) {
        if (ver) { std::wcout << "\n"; }

        std::stringstream ss;
        ss << visible_options;
        std::string desc(ss.str());
        desc.pop_back();

        std::wcout << "Usage: " << argv[0] << " [OPTION]... [path to EVE logs]\n\n"
                   << desc.c_str() << std::endl;
    }

    if (ver || help) { std::exit(0); }

    return { vm.count("verbose") != 0, vm.count("force") == 0, vm["regex"].as<std::wstring>(),
             vm["cache"].as<std::wstring>(), std::move(log_path), parseHosts(vm) };
}
