// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include "nlohmann_json.h"

module nom.json_config;

import std;

namespace nom::jcfg
{
    namespace
    {
        std::string append_pointer(const std::string& pointer, std::string_view token)
        {
            std::string result = pointer + '/';
            for (const char character : token)
            {
                if (character == '~')
                {
                    result += "~0";
                }
                else if (character == '/')
                {
                    result += "~1";
                }
                else
                {
                    result += character;
                }
            }
            return result;
        }

        bool is_placeholder_space(char character)
        {
            return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
                   character == '\f' || character == '\v';
        }

        std::string substitute_string(const std::string& text,
                                      const environment_lookup& environment,
                                      const config_location& location,
                                      const std::string& pointer)
        {
            constexpr std::string_view prefix = "{jcfg:run-var";
            std::string result;
            std::size_t consumed = 0;
            while (true)
            {
                const auto start = text.find(prefix, consumed);
                if (start == std::string::npos)
                {
                    result.append(text, consumed, std::string::npos);
                    return result;
                }
                result.append(text, consumed, start - consumed);
                auto name_start = start + prefix.size();
                if (name_start == text.size() || !is_placeholder_space(text[name_start]))
                {
                    throw config_error(
                        config_stage::substitution, "malformed runtime placeholder", location.path, pointer);
                }
                while (name_start < text.size() && is_placeholder_space(text[name_start]))
                {
                    ++name_start;
                }
                auto end = name_start;
                while (end < text.size() && text[end] != '}' && text[end] != '{' && !is_placeholder_space(text[end]))
                {
                    ++end;
                }
                if (end == name_start || end == text.size() || text[end] != '}')
                {
                    throw config_error(
                        config_stage::substitution, "malformed runtime placeholder", location.path, pointer);
                }
                const auto name = std::string_view(text).substr(name_start, end - name_start);
                const auto value = environment(name);
                if (!value)
                {
                    throw config_error(config_stage::substitution,
                                       "unset runtime environment variable '" + std::string(name) + "'",
                                       location.path,
                                       pointer);
                }
                result += *value;
                consumed = end + 1;
            }
        }

        void substitute_values(nlohmann::json& document,
                               const environment_lookup& environment,
                               const config_location& location,
                               const std::string& pointer)
        {
            if (document.is_string())
            {
                document = substitute_string(document.get_ref<const std::string&>(), environment, location, pointer);
            }
            else if (document.is_object())
            {
                for (auto member = document.begin(); member != document.end(); ++member)
                {
                    substitute_values(member.value(), environment, location, append_pointer(pointer, member.key()));
                }
            }
            else if (document.is_array())
            {
                for (std::size_t index = 0; index < document.size(); ++index)
                {
                    substitute_values(
                        document[index], environment, location, append_pointer(pointer, std::to_string(index)));
                }
            }
        }

        std::optional<std::string> try_override(nlohmann::json& document,
                                                const nlohmann::json::json_pointer& pointer,
                                                const nlohmann::json& replacement,
                                                const mapping_probe& probe)
        {
            auto candidate = document;
            candidate.at(pointer) = replacement;
            try
            {
                probe(candidate);
            }
            catch (const std::bad_alloc&)
            {
                throw;
            }
            catch (const std::exception& error)
            {
                return error.what();
            }
            document = std::move(candidate);
            return std::nullopt;
        }
    } // namespace

    nlohmann::json read_config(const config_location& location)
    {
        if (!location.path)
        {
            if (location.source != config_source::hard_wired)
            {
                throw config_error(config_stage::reading, "selected config has no file path");
            }
            return nlohmann::json::object();
        }
        if (location.path->empty() || location.path->native().find('\0') != std::string::npos)
        {
            throw config_error(
                config_stage::reading, "config path must be nonempty and contain no null characters", location.path);
        }

        std::error_code error;
        if (!std::filesystem::is_regular_file(*location.path, error))
        {
            throw config_error(config_stage::reading,
                               error ? "cannot access config file: " + error.message() : "config is not a regular file",
                               location.path);
        }
        std::ifstream input(*location.path, std::ios::binary);
        if (!input)
        {
            throw config_error(config_stage::reading, "cannot open config file", location.path);
        }

        std::string contents;
        std::array<char, 8192> buffer{};
        while (input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            contents.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
        }
        if (input.bad() || !input.eof())
        {
            throw config_error(config_stage::reading, "cannot read config file", location.path);
        }

        nlohmann::json document;
        try
        {
            document = nlohmann::json::parse(contents);
        }
        catch (const nlohmann::json::exception& parse_error)
        {
            throw config_error(config_stage::parsing, parse_error.what(), location.path);
        }
        if (!document.is_object())
        {
            throw config_error(config_stage::parsing, "config root must be a JSON object", location.path);
        }
        return document;
    }

    void substitute_runtime_variables(nlohmann::json& document,
                                      const environment_lookup& environment,
                                      const config_location& location)
    {
        substitute_values(document, environment, location, {});
    }

    void apply_config_overrides(nlohmann::json& document,
                                std::span<const config_override> overrides,
                                const mapping_probe& probe,
                                const config_location& location)
    {
        for (const auto& override : overrides)
        {
            nlohmann::json::json_pointer pointer;
            try
            {
                pointer = nlohmann::json::json_pointer(override.pointer);
                static_cast<void>(document.at(pointer));
            }
            catch (const nlohmann::json::exception& error)
            {
                throw config_error(config_stage::overrides, error.what(), location.path, override.pointer);
            }

            const auto string_error = try_override(document, pointer, override.value, probe);
            if (!string_error)
            {
                continue;
            }

            nlohmann::json replacement;
            try
            {
                replacement = nlohmann::json::parse(override.value);
            }
            catch (const nlohmann::json::exception& error)
            {
                throw config_error(config_stage::overrides,
                                   "override is neither a mapped string nor a JSON value; string mapping: " +
                                       *string_error + "; JSON parsing: " + error.what(),
                                   location.path,
                                   override.pointer);
            }
            if (const auto value_error = try_override(document, pointer, replacement, probe))
            {
                throw config_error(config_stage::overrides,
                                   "override does not map to the config; string mapping: " + *string_error +
                                       "; JSON mapping: " + *value_error,
                                   location.path,
                                   override.pointer);
            }
        }
    }
} // namespace nom::jcfg
