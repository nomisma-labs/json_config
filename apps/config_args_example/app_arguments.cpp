// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include "app_arguments.h"

#include <boost/program_options.hpp>

import std;

namespace example
{
    app_arguments parse_app_arguments(const std::vector<std::string>& arguments)
    {
        namespace po = boost::program_options;
        app_arguments result;
        po::options_description description("Application options");
        description.add_options()("help,h", po::bool_switch(&result.help), "Print help")(
            "name", po::value(&result.name)->default_value("world"), "Greeting recipient; also accepted positionally")(
            "count", po::value(&result.count)->default_value(1), "Number of greetings")(
            "verbose,v", po::bool_switch(&result.verbose), "Print application progress");

        po::positional_options_description positional;
        positional.add("name", 1);
        po::variables_map variables;
        po::store(po::command_line_parser(arguments).options(description).positional(positional).run(), variables);
        po::notify(variables);

        if (result.help)
        {
            std::cout << "Usage: config_args_example [options] [name]\n"
                      << description
                      << "\nConfig options:\n"
                         "  --config FILE       Select a JSON config\n"
                         "  --set POINTER=value Override a config value; repeatable\n"
                         "  --check-config      Validate config and exit before running\n";
            return result;
        }
        if (result.count <= 0)
        {
            throw std::invalid_argument("--count must be positive");
        }
        if (result.name.empty())
        {
            throw std::invalid_argument("--name must not be empty");
        }
        return result;
    }
} // namespace example
