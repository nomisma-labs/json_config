// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: MIT

export module nom.json_config:mapping;

import std;

import nom.config_types;

// Keep JSON adapters with the types' associated namespaces for nlohmann ADL.
export namespace nom
{
    namespace detail
    {
        // Validate the source before asking nlohmann to perform its conversion.
        template <typename Json, typename Value> void from_json(const Json& document, config_number<Value>& destination)
        {
            if constexpr (std::same_as<Value, double>)
            {
                if (!document.is_number())
                {
                    throw Json::type_error::create(302, "expected a number, got " + document.dump(), &document);
                }
                const auto value = document.template get<double>();
                if (!std::isfinite(value))
                {
                    throw Json::out_of_range::create(
                        406, "expected a finite number, got " + std::to_string(value), &document);
                }
                destination.value = value;
            }
            else
            {
                if (!document.is_number_integer())
                {
                    throw Json::type_error::create(302, "expected an integer, got " + document.dump(), &document);
                }
                if constexpr (std::same_as<Value, std::int64_t>)
                {
                    if (document.is_number_unsigned() &&
                        document.template get<std::uint64_t>() > std::numeric_limits<std::int64_t>::max())
                    {
                        throw Json::out_of_range::create(
                            406, "integer exceeds the signed 64-bit range, got " + document.dump(), &document);
                    }
                }
                else
                {
                    if (!document.is_number_unsigned() && document.template get<std::int64_t>() < 0)
                    {
                        throw Json::out_of_range::create(
                            406, "expected a nonnegative integer, got " + document.dump(), &document);
                    }
                }
                destination.value = document.template get<Value>();
            }
        }

        template <typename Json, typename Value> void to_json(Json& document, const config_number<Value>& source)
        {
            if constexpr (std::same_as<Value, double>)
            {
                if (!std::isfinite(source.value))
                {
                    throw Json::out_of_range::create(
                        406, "expected a finite number, got " + std::to_string(source.value), &document);
                }
            }
            document = source.value;
        }
    } // namespace detail

    template <typename Json, typename Value> void from_json(const Json& document, cfg_defaulted<Value>& destination)
    {
        if (!document.is_null())
        {
            document.get_to(destination.value);
        }
    }

    template <typename Json, typename Value> void to_json(Json& document, const cfg_defaulted<Value>& source)
    {
        document = source.value;
    }

} // namespace nom
