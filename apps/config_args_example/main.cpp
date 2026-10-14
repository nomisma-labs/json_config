// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include "app_arguments.h"

#include <json_config/config.h>

#include <exception>
#include <expected>
#include <iostream>
#include <string>

import nom.json_config;

namespace example
{
    struct app_config
    {
        nom::jcfg::defaulted<std::string> greeting{"Hello"};

        std::expected<void, std::string> validate() const
        {
            if (greeting->empty())
            {
                return std::unexpected("greeting must not be empty");
            }
            return {};
        }
    };

    NOM_CONFIG(app_config, greeting)
} // namespace example

int main(int argc, char** argv)
{
    try
    {
        const auto arguments = nom::jcfg::parse_config_arguments(argc, argv, NOM_CONFIG_KEY_VALUE);
        const auto app = example::parse_app_arguments(arguments.remaining_arguments);
        if (app.help)
        {
            return 0;
        }

        const auto config = nom::jcfg::load_config<example::app_config>(arguments.options);
        if (arguments.options.check_config)
        {
            std::cout << "Config OK\n";
            return 0;
        }

        if (app.verbose)
        {
            std::cout << "Greeting count: " << app.count << '\n';
        }
        for (int index = 0; index < app.count; ++index)
        {
            std::cout << *config.value.greeting << ", " << app.name << "!\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
