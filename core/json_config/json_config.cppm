// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include "nlohmann_json.h"

#include <concepts>
#include <exception>
#include <expected>
#include <new>
#include <optional>
#include <string>
#include <utility>

export module nom.json_config;

export import nom.config_types;
export import :mapping;
export import :runtime;

export namespace nom::jcfg
{
    namespace detail
    {
        template <typename Config>
        Config map_loaded_config(const nlohmann::json& document, const config_location& location)
        {
            try
            {
                return document.template get<Config>();
            }
            catch (const std::bad_alloc&)
            {
                throw;
            }
            catch (const std::exception& error)
            {
                throw config_error(config_stage::mapping, error.what(), location.path);
            }
        }

        // NOM_JCONFIG supplies nlohmann::json; these helpers only handle presence.
        template <typename Json, typename Value>
        void read_member(const Json& document, const char* name, Value& destination)
        {
            document.at(name).get_to(destination);
        }

        template <typename Json, typename Value>
        void read_member(const Json& document, const char* name, cfg_defaulted<Value>& destination)
        {
            if (const auto found = document.find(name); found != document.end())
            {
                found->get_to(destination);
            }
        }

        template <typename Json, typename Value>
        void read_member(const Json& document, const char* name, std::optional<Value>& destination)
        {
            const auto found = document.find(name);
            if (found == document.end())
            {
                return;
            }
            if (found->is_null())
            {
                destination.reset();
            }
            else
            {
                if (!destination)
                {
                    destination.emplace();
                }
                found->get_to(*destination);
            }
        }
    } // namespace detail

    template <typename Config> struct loaded_config
    {
        Config value;
        config_location location;
    };

    template <typename Config>
    loaded_config<Config> load_config(const config_options& options = {},
                                      const environment_lookup& environment = process_environment)
    {
        static_assert(
            requires(const Config& value) {
                { value.validate() } -> std::same_as<std::expected<void, std::string>>;
            }, "The root config must provide validate() const returning std::expected<void, std::string>");

        auto location = locate_config(options, environment);
        auto document = read_config(location);
        substitute_runtime_variables(document, environment, location);
        if (location.source == config_source::hard_wired && !options.overrides.empty())
        {
            // C++ defaults provide the existing JSON targets for hard-wired overrides.
            auto initial = detail::map_loaded_config<Config>(document, location);
            try
            {
                document = initial;
            }
            catch (const std::bad_alloc&)
            {
                throw;
            }
            catch (const std::exception& error)
            {
                throw config_error(config_stage::mapping, error.what(), location.path);
            }
        }
        apply_config_overrides(
            document,
            options.overrides,
            [](const nlohmann::json& candidate) { (void)candidate.template get<Config>(); },
            location);
        auto value = detail::map_loaded_config<Config>(document, location);
        std::expected<void, std::string> validation;
        try
        {
            validation = std::as_const(value).validate();
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            throw config_error(config_stage::validation, error.what(), location.path);
        }
        if (!validation)
        {
            throw config_error(config_stage::validation, validation.error(), location.path);
        }
        return {std::move(value), std::move(location)};
    }
} // namespace nom::jcfg
