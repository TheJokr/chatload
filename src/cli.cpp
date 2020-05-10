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
#include <type_traits>

// Boost
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

// chatload components
#include "common.hpp"
#include "constants.hpp"
#include "buildinfo.hpp"


namespace po = boost::program_options;
namespace clcfg = chatload::constants;

namespace {
template<class T>
po::typed_value<T, chatload::char_t>* po_value_t(T* v = nullptr) {
    // See po::value()/po::wvalue. Ownership is passed to po::options_description.
    return new po::typed_value<T, chatload::char_t>(v);
}

void parseConfig(const chatload::string& file, const po::options_description& cfg_options, po::variables_map& vm) {
    std::basic_ifstream<chatload::char_t> in(file);
    po::store(po::parse_config_file(in, cfg_options), vm);
}

std::vector<chatload::cli::host> parseHosts(const po::variables_map& vm) {
    std::vector<chatload::cli::host> hosts;

    const auto hosts_iter = vm.find("network.host");
    if (hosts_iter != vm.end()) {
        const auto& host_lits = hosts_iter->second.as<std::vector<std::string>>();
        for (const auto& host_lit : host_lits) {
            // Not UB! (see std::basic_string::operator[])
            const bool insecure = host_lit[0] == '?';

            // Strip insecure marker (if present)
            boost::string_view host(host_lit);
            host.remove_prefix(static_cast<boost::string_view::size_type>(insecure));
            if (host.empty()) { continue; }

            boost::string_view hostname;
            if (host[0] == '[') {
                // IPv6 address
                const auto name_end = host.find(']', 1);
                if (name_end == boost::string_view::npos) {
                    // Address block not closed
                    continue;
                }

                // Substring between brackets (from after [ to before ])
                hostname = host.substr(1, name_end - 1);
                // Substring after : (may be empty)
                host.remove_prefix(name_end + 2);
            } else {
                const auto name_end = host.find(':');
                if (name_end == boost::string_view::npos) {
                    hostname = host;
                    host.clear();
                } else {
                    hostname = host.substr(0, name_end);
                    host.remove_prefix(name_end + 1);
                }
            }

            if (hostname.empty()) { continue; }
            hosts.push_back({ insecure, std::string{hostname},
                              host.empty() ? clcfg::DEFAULT_PORT : std::string{host} });
        }
    }

    if (hosts.empty()) {
        hosts.push_back({ false, clcfg::DEFAULT_HOST, clcfg::DEFAULT_PORT });
    }

    return hosts;
}
}  // Anonymous namespace


// NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
chatload::cli::options chatload::cli::parseArgs(int argc, chatload::char_t* argv[]) {
    using timeout_t = std::remove_cv_t<decltype(clcfg::DEFAULT_TIMEOUT)>;

    // Options available both on the CLI and in the config
    boost::optional<std::string> ca_file, ca_path, cipher_list, ciphersuites;
    boost::optional<chatload::string> cache_file;
    po::options_description generic_options("Options also available in the config file");
    generic_options.add_options()
        ("verbose,v", po::bool_switch(), "list read logs")
        ("force,f", po::bool_switch(), "read all logs, even if they have been read before")
        ("insecure,k", po::bool_switch(), "allow TLS connections with invalid certificates")
        ("timeout,t", po::value<timeout_t>()->default_value(clcfg::DEFAULT_TIMEOUT), "network timeout (in seconds)")
        ("cafile", po::value(&ca_file), "PEM file with trusted CA certificate(s)")
        ("capath", po::value(&ca_path), "directory with trusted PEM CA certificate(s)")
        ("ciphers", po::value(&cipher_list), "TLSv1.2 ciphers to use (OpenSSL format)")
        ("ciphersuites", po::value(&ciphersuites), "TLSv1.3 ciphers to use (OpenSSL format)")
        ("cache", po_value_t(&cache_file), "cache file");

    // Options available on the CLI
    po::options_description visible_options("Allowed options");
    visible_options.add(generic_options);
    visible_options.add_options()
        ("help,h", po::bool_switch(), "display this help message and exit")
        ("version,V", po::bool_switch(), "display version information and exit")
        ("config,c", po_value_t<chatload::string>()->default_value(clcfg::CONFIG_FILE, clcfg::CONFIG_HELP), "config file");

    // Options parsed from the CLI (including positional ones)
    boost::optional<chatload::string> log_folder;
    po::options_description cli_options;
    cli_options.add(visible_options);
    cli_options.add_options()("log-path", po_value_t(&log_folder));

    po::positional_options_description pos_options;
    pos_options.add("log-path", 1);

    // Options available in the config
    po::options_description cfg_options;
    cfg_options.add(generic_options);
    cfg_options.add_options()
        ("network.host", po::value<std::vector<std::string>>())
        ("regex", po_value_t<chatload::string>()->default_value(CHATLOAD_STRING(".*"), ".*"));

    po::variables_map vm;
    po::store(po::basic_command_line_parser<chatload::char_t>(argc, argv)
                  .options(cli_options).positional(pos_options).run(), vm);
    parseConfig(vm["config"].as<chatload::string>(), cfg_options, vm);
    po::notify(vm);

    bool ver = vm["version"].as<bool>(), help = vm["help"].as<bool>();
    if (ver || help) {
        // Remove path to executable if present (compact output)
        boost::basic_string_view<chatload::char_t> path(argv[0]);
        const auto path_end = path.rfind(chatload::PATH_SEP);
        if (path_end != boost::basic_string_view<chatload::char_t>::npos) {
            path.remove_prefix(path_end + 1);
        }

        if (ver) {
            // GIT_SHA1 may be empty (i.e., only contains \0), in which case it's not reported
            chatload::cout << path << " " << chatload::VERSION << " built at " << chatload::BUILD_TIME
                           << " for " << chatload::REPO_SLUG << "\n";
            if (chatload::GIT_SHA1[0]) { chatload::cout << "Git commit SHA1: " << chatload::GIT_SHA1 << "\n\n"; }

            chatload::cout << "Copyright (C) 2015-2019  Leo Bloecher\n"
                           << "This program comes with ABSOLUTELY NO WARRANTY.\n"
                           << "This is free software, and you are welcome to redistribute it under certain conditions."
                           << std::endl;
        }

        if (help) {
            if (ver) { chatload::cout << "\n"; }

            std::stringstream ss;
            ss << visible_options;
            std::string desc = ss.str();
            desc.pop_back();

            chatload::cout << "Usage: " << path << " [OPTION]... [path to EVE logs]\n\n"
                           << desc.c_str() << std::endl;
        }

        std::exit(0);
    }

    return { vm["verbose"].as<bool>(), !vm["force"].as<bool>(), vm["insecure"].as<bool>(),
             boost::posix_time::seconds(vm["timeout"].as<timeout_t>()), std::move(ca_file), std::move(ca_path),
             std::move(cipher_list), std::move(ciphersuites), vm["regex"].as<chatload::string>(),
             std::move(cache_file), std::move(log_folder), parseHosts(vm) };
}
