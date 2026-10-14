// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

export module nom.json_config:runtime;

export namespace nom::jcfg
{
    enum class config_stage
    {
        arguments,
        location,
        reading,
        parsing,
        substitution,
        overrides,
        mapping,
        validation
    };

    enum class config_source
    {
        command_line,
        environment_file,
        executable_directory,
        search_directory,
        hard_wired
    };

    std::string_view config_stage_name(config_stage stage) noexcept;
    std::string_view config_source_name(config_source source) noexcept;

    class config_error : public std::runtime_error
    {
    public:
        config_error(config_stage stage,
                     std::string message,
                     std::optional<std::filesystem::path> file = {},
                     std::string pointer = {});

        config_stage stage;
        std::optional<std::filesystem::path> file;
        std::string pointer;
    };

    struct config_override
    {
        std::string pointer;
        std::string value;
    };

    struct config_options
    {
        std::filesystem::path executable_path;
        std::string config_key;
        std::optional<std::filesystem::path> config_file;
        std::vector<config_override> overrides;
        bool check_config{};
    };

    struct config_location
    {
        std::optional<std::filesystem::path> path;
        config_source source{config_source::hard_wired};
    };

    struct parsed_config_arguments
    {
        config_options options;
        std::vector<std::string> remaining_arguments;
    };

    using environment_lookup = std::function<std::optional<std::string>(std::string_view)>;
    using mapping_probe = std::function<void(const nlohmann::json&)>;

    std::optional<std::string> process_environment(std::string_view name);

    // The span contains arguments after argv[0]; unrecognized arguments are retained.
    parsed_config_arguments parse_config_arguments(std::span<const std::string_view> arguments,
                                                   std::string config_key = {});
    parsed_config_arguments parse_config_arguments(int argc, const char* const* argv, std::string config_key = {});

    std::filesystem::path current_executable_path();
    config_location locate_config(const config_options& options,
                                  const environment_lookup& environment = process_environment);

    nlohmann::json read_config(const config_location& location);
    void substitute_runtime_variables(nlohmann::json& document,
                                      const environment_lookup& environment = process_environment,
                                      const config_location& location = {});
    void apply_config_overrides(nlohmann::json& document,
                                std::span<const config_override> overrides,
                                const mapping_probe& probe,
                                const config_location& location = {});
} // namespace nom::jcfg
