/*
 * chatload: Log reader to collect EVE Online character names
 * Copyright (C) 2015-2018  Leo Blöcher
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

// Streams
#include <sstream>

// Containers
#include <string>

// Boost
#include <boost/program_options.hpp>

// chatload components
#include "constants.hpp"
#include "exception.hpp"


namespace po = boost::program_options;

chatload::cli::options chatload::cli::parseArgs(int argc, wchar_t* argv[]) {
    namespace clcfg = chatload::constants;

    po::options_description visible_options("Allowed options");
    visible_options.add_options()
        ("help,h", "display this help text and exit")
        ("version,V", "display version information and exit")
        ("verbose,v", "display read logs")
        ("force,f", "read all logs instead of only unknown ones")
        ("config", po::wvalue<std::wstring>()->default_value(clcfg::CONFIGFILE, clcfg::CONFIG_HELP), "config file")
        ("cache", po::wvalue<std::wstring>()->default_value(clcfg::CACHEFILE, clcfg::CACHE_HELP), "cache file");

    po::options_description cmd_options;
    cmd_options.add(visible_options);
    cmd_options.add_options()
        ("log-path", po::wvalue<std::wstring>()->default_value(std::wstring(), std::string()), "path to logs");

    po::positional_options_description pos_options;
    pos_options.add("log-path", 1);

    po::variables_map vm;
    po::wcommand_line_parser prs(argc, argv);
    prs.options(cmd_options).positional(pos_options).allow_unregistered();
    po::store(prs.run(), vm);
    po::notify(vm);

    std::wostringstream out;
    bool ver = vm.count("version"), help = vm.count("help");
    if (ver) {
        out << argv[0] << " version " << chatload::VERSION << "\n"
            << "Copyright (C) 2015-2018  Leo Bloecher\n"
            << "This program comes with ABSOLUTELY NO WARRANTY.\n"
            << "This is free software, and you are welcome to redistribute it under certain conditions.";
    }

    if (help) {
        if (ver) { out << "\n\n"; }

        std::stringstream ss;
        ss << visible_options;

        out << "Usage: " << argv[0] << " [OPTION]... [path to EVE logs]\n\n"
            << ss.str().c_str();
    }

    if (ver || help) { throw chatload::runtime_error(out.str()); }

    return { vm.count("verbose") != 0, vm.count("force") == 0, vm["config"].as<std::wstring>(),
             vm["cache"].as<std::wstring>(), vm["log-path"].as<std::wstring>() };
}
