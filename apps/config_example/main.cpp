// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <json_config/config.h>

#include <expected>
#include <iostream>
#include <optional>
#include <string>

import nom.json_config;

namespace example
{
    using namespace nom::jcfg::types;

    struct app_config
    {
        defaulted<std::string> endpoint{"https://example.test/feed"};
        defaulted<jint> timeout_s{10};
        defaulted<bool> enabled{true};
        std::optional<std::string> interface_name;

        std::expected<void, std::string> validate() const
        {
            if (endpoint->empty() || *timeout_s <= 0)
            {
                return std::unexpected("endpoint must be nonempty and timeout_s must be positive");
            }
            return {};
        }
    };

    NOM_CONFIG(app_config, endpoint, timeout_s, enabled, interface_name)
} // namespace example

int main(int argc, char** argv)
{
    try
    {
        const auto arguments = nom::jcfg::parse_config_arguments(argc, argv, NOM_CONFIG_KEY_VALUE);
        if (!arguments.remaining_arguments.empty())
        {
            std::cerr << "Usage: config_example [--config FILE] [--set POINTER=value] [--check-config]\n";
            return 1;
        }
        const auto loaded = nom::jcfg::load_config<example::app_config>(arguments.options);
        std::cout << "Config OK (" << nom::jcfg::config_source_name(loaded.location.source) << ')';
        if (loaded.location.path)
        {
            std::cout << ": " << loaded.location.path->string();
        }
        std::cout << '\n';
        if (arguments.options.check_config)
        {
            return 0;
        }

        // Real application services would be initialized here using loaded.value.
        std::cout << "Application initialized; timeout: " << *loaded.value.timeout_s << " seconds\n";
        return 0;
    }
    catch (const nom::jcfg::config_error& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
