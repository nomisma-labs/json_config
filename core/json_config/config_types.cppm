// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

export module nom.json_config:types;

export namespace nom::jcfg
{
    namespace detail
    {
        template <typename Value> struct number
        {
            static_assert(std::same_as<Value, std::int64_t> || std::same_as<Value, std::uint64_t> ||
                          std::same_as<Value, double>);

            Value value{};

            constexpr operator Value() const noexcept
            {
                return value;
            }

            constexpr Value& get() noexcept
            {
                return value;
            }
            constexpr const Value& get() const noexcept
            {
                return value;
            }
        };

        // Validate the source before asking nlohmann to perform its conversion.
        template <typename Json, typename Value> void from_json(const Json& document, number<Value>& destination)
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

        template <typename Json, typename Value> void to_json(Json& document, const number<Value>& source)
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

    using jint = detail::number<std::int64_t>;
    using juint = detail::number<std::uint64_t>;
    using jdouble = detail::number<double>;

    template <typename Value> struct defaulted
    {
        Value value{};

        constexpr operator Value&() noexcept
        {
            return value;
        }
        constexpr operator const Value&() const noexcept
        {
            return value;
        }

        constexpr Value* operator->() noexcept
        {
            return &value;
        }
        constexpr const Value* operator->() const noexcept
        {
            return &value;
        }

        constexpr Value& operator*() noexcept
        {
            return value;
        }
        constexpr const Value& operator*() const noexcept
        {
            return value;
        }

        constexpr Value& get() noexcept
        {
            return value;
        }
        constexpr const Value& get() const noexcept
        {
            return value;
        }
    };

    template <typename Json, typename Value> void from_json(const Json& document, defaulted<Value>& destination)
    {
        if (!document.is_null())
        {
            document.get_to(destination.value);
        }
    }

    template <typename Json, typename Value> void to_json(Json& document, const defaulted<Value>& source)
    {
        document = source.value;
    }

    namespace types
    {
        using nom::jcfg::defaulted;
        using nom::jcfg::jdouble;
        using nom::jcfg::jint;
        using nom::jcfg::juint;
    } // namespace types

    // Explicit opt-in for application-owned types with nlohmann ADL converters.
    template <typename Value> inline constexpr bool enable_config_type = false;

    namespace detail
    {
        template <typename Value> std::false_type nom_config_type(Value*);

        template <typename Value>
        inline constexpr bool is_config_value =
            enable_config_type<Value> || decltype(nom_config_type(static_cast<Value*>(nullptr)))::value;

        template <> inline constexpr bool is_config_value<bool> = true;
        template <> inline constexpr bool is_config_value<std::string> = true;
        template <typename Value> inline constexpr bool is_config_value<number<Value>> = true;
        template <typename Value> inline constexpr bool is_config_value<defaulted<Value>> = is_config_value<Value>;
        template <typename Value, typename Allocator>
        inline constexpr bool is_config_value<std::vector<Value, Allocator>> = is_config_value<Value>;
        template <typename Value, typename Compare, typename Allocator>
        inline constexpr bool is_config_value<std::map<std::string, Value, Compare, Allocator>> =
            is_config_value<Value>;
        template <typename Value, typename Hash, typename Equal, typename Allocator>
        inline constexpr bool is_config_value<std::unordered_map<std::string, Value, Hash, Equal, Allocator>> =
            is_config_value<Value>;

        template <typename Value> inline constexpr bool is_config_member = is_config_value<Value>;
        template <typename Value> inline constexpr bool is_config_member<std::optional<Value>> = is_config_value<Value>;
    } // namespace detail
} // namespace nom::jcfg
