// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include "util/os.h"

module nom.json_config;

import std;

namespace nom::jcfg
{
    namespace
    {
        namespace fs = std::filesystem;

        fs::path absolute_config_path(const fs::path& path)
        {
            if (path.empty() || path.native().find('\0') != std::string::npos)
            {
                throw config_error(
                    config_stage::location, "config path must be nonempty and contain no null characters", path);
            }
            std::error_code error;
            auto result = fs::absolute(path, error);
            if (error)
            {
                throw config_error(config_stage::location, "cannot resolve config path: " + error.message(), path);
            }
            return result;
        }

        std::optional<config_location> select_config_file(const fs::path& path, config_source source, bool required)
        {
            std::error_code error;
            const auto entry = fs::symlink_status(path, error);
            if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(entry)))
            {
                if (required)
                {
                    throw config_error(config_stage::location, "selected config file does not exist", path);
                }
                return std::nullopt;
            }
            if (error)
            {
                throw config_error(config_stage::location, "cannot inspect config file: " + error.message(), path);
            }
            const auto status = fs::status(path, error);
            if (error)
            {
                throw config_error(config_stage::location, "cannot access config file: " + error.message(), path);
            }
            if (!fs::is_regular_file(status))
            {
                throw config_error(config_stage::location, "config path is not a regular file", path);
            }
            const std::ifstream input(path, std::ios::binary);
            if (!input)
            {
                throw config_error(config_stage::location, "cannot read config file", path);
            }
            return config_location{path, source};
        }

        std::vector<fs::path> config_search_directories(std::string_view text, const fs::path& executable_directory)
        {
            std::vector<fs::path> directories;
            std::size_t begin{};
            while (true)
            {
                const auto end = text.find(':', begin);
                const auto directory = text.substr(begin, end == std::string_view::npos ? end : end - begin);
                if (directory.empty())
                {
                    throw config_error(config_stage::location, "NOM_CONFIG_DIR contains an empty directory entry");
                }
                fs::path path{directory};
                if (path.is_relative())
                {
                    path = executable_directory / path;
                }
                directories.push_back(absolute_config_path(path));
                if (end == std::string_view::npos)
                {
                    return directories;
                }
                begin = end + 1;
            }
        }
    } // namespace

    std::filesystem::path current_executable_path()
    {
        try
        {
            return os::executable_path();
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            throw config_error(config_stage::location, "cannot locate executable: " + std::string(error.what()));
        }
    }

    config_location locate_config(const config_options& options, const environment_lookup& environment)
    {
        if (options.config_file)
        {
            return *select_config_file(absolute_config_path(*options.config_file), config_source::command_line, true);
        }
        if (const auto file = environment("NOM_CONFIG"))
        {
            return *select_config_file(absolute_config_path(*file), config_source::environment_file, true);
        }

        const auto executable =
            absolute_config_path(options.executable_path.empty() ? current_executable_path() : options.executable_path);
        const auto directory = executable.parent_path();
        const auto key = options.config_key.empty() ? executable.filename().string() : options.config_key;
        if (key.empty() || key == "." || key == ".." || key.find('/') != std::string::npos ||
            key.find('\0') != std::string::npos)
        {
            throw config_error(config_stage::location, "config key must be a nonempty filename component");
        }
        const auto filename = key + ".json";
        if (const auto selected = select_config_file(directory / filename, config_source::executable_directory, false))
        {
            return *selected;
        }
        if (const auto search_path = environment("NOM_CONFIG_DIR"))
        {
            for (const auto& search_directory : config_search_directories(*search_path, directory))
            {
                for (const auto& candidate : {search_directory / filename, search_directory / key / filename})
                {
                    if (const auto selected = select_config_file(candidate, config_source::search_directory, false))
                    {
                        return *selected;
                    }
                }
            }
        }
        return {};
    }
} // namespace nom::jcfg
