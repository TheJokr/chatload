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
#include "common.hpp"
#include "constants.hpp"


namespace po = boost::program_options;

namespace {
void parseConfig(const chatload::string& file, po::variables_map& vm) {
    std::basic_ifstream<chatload::char_t> in(file);
    if (in) {
        po::options_description cfg_options;

        // See value()/wvalue(). Ownership is passed to cfg_options.
        auto* regex_val = new po::typed_value<chatload::string, chatload::char_t>(0);
        cfg_options.add_options()
            ("network.host", po::value<std::vector<std::string>>())
            ("regex", regex_val->default_value(CHATLOAD_STRING(".*"), ".*"));

        po::store(po::parse_config_file(in, cfg_options), vm);
    } else {
        vm.emplace(std::string("regex"), po::variable_value(chatload::string(CHATLOAD_STRING(".*")), true));
    }
}

std::vector<chatload::cli::host> parseHosts(po::variables_map& vm) {
    std::vector<chatload::cli::host> hosts;

    if (vm.count("network.host")) {
        const auto& host_lits = vm["network.host"].as<std::vector<std::string>>();
        for (const auto& host_lit : host_lits) {
            auto name_end = host_lit.find(':');
            if (name_end == std::string::npos) {
                hosts.push_back({ host_lit, chatload::DEFAULTPORT });
            } else {
                hosts.push_back({ host_lit.substr(0, name_end), host_lit.substr(name_end + 1) });
            }
        }
    } else {
        hosts.push_back({ chatload::DEFAULTHOST, chatload::DEFAULTPORT });
    }

    return hosts;
}
}  // Anonymous namespace


chatload::cli::options chatload::cli::parseArgs(int argc, chatload::char_t* argv[]) {
    namespace clcfg = chatload::constants;

    po::options_description visible_options("Allowed options");
    {
        // See value()/wvalue(). Ownership is passed to visible_options.
        auto* config_val = new po::typed_value<chatload::string, chatload::char_t>(0);
        auto* cache_val = new po::typed_value<chatload::string, chatload::char_t>(0);
        visible_options.add_options()
            ("help,h", "display this help text and exit")
            ("version,V", "display version information and exit")
            ("verbose,v", "list read logs")
            ("force,f", "read all logs, even if they have been read before")
            ("config", config_val->default_value(clcfg::CONFIGFILE, clcfg::CONFIG_HELP), "config file")
            ("cache", cache_val->default_value(clcfg::CACHEFILE, clcfg::CACHE_HELP), "cache file");
    }

    boost::optional<chatload::string> log_path;
    po::options_description cli_options;
    cli_options.add(visible_options);
    {
        // See value()/wvalue(). Ownership is passed to cli_options.
        auto* log_path_val = new po::typed_value<decltype(log_path), chatload::char_t>(&log_path);
        cli_options.add_options()("log-path", log_path_val, "path to logs");
    }

    po::positional_options_description pos_options;
    pos_options.add("log-path", 1);

    po::variables_map vm;
    po::store(po::basic_command_line_parser<chatload::char_t>(argc, argv)
                  .options(cli_options).positional(pos_options).run(), vm);
    parseConfig(vm["config"].as<chatload::string>(), vm);
    po::notify(vm);

    bool ver = vm.count("version"), help = vm.count("help");
    if (ver) {
        CHATLOAD_COUT << argv[0] << " version " << chatload::VERSION << "\n"
                      << "Copyright (C) 2015-2019  Leo Bloecher\n"
                      << "This program comes with ABSOLUTELY NO WARRANTY.\n"
                      << "This is free software, and you are welcome to redistribute it under certain conditions."
                      << std::endl;
    }

    if (help) {
        if (ver) { CHATLOAD_COUT << "\n"; }

        std::stringstream ss;
        ss << visible_options;
        std::string desc(ss.str());
        desc.pop_back();

        CHATLOAD_COUT << "Usage: " << argv[0] << " [OPTION]... [path to EVE logs]\n\n"
                      << desc.c_str() << std::endl;
    }

    if (ver || help) { std::exit(0); }

    return { vm.count("verbose") != 0, vm.count("force") == 0, vm["regex"].as<chatload::string>(),
             vm["cache"].as<chatload::string>(), std::move(log_path), parseHosts(vm) };
}
