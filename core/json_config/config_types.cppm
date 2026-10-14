// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

export module nom.config_types;

export namespace nom
{
    namespace detail
    {
        template <typename Value> struct config_number
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
    } // namespace detail

    using cfg_int = detail::config_number<std::int64_t>;
    using cfg_uint = detail::config_number<std::uint64_t>;
    using cfg_double = detail::config_number<double>;
    using cfg_bool = bool;
    using cfg_string = std::string;

    template <typename Value> using cfg_vector = std::vector<Value>;
    template <typename Value> using cfg_map = std::map<std::string, Value>;
    template <typename Value> using cfg_unordered_map = std::unordered_map<std::string, Value>;
    template <typename Value> using cfg_optional = std::optional<Value>;

    // Carries an application default; a format adapter decides when to retain it.
    template <typename Value> struct cfg_defaulted
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

    namespace cfg_types
    {
        using ::nom::cfg_bool;
        using ::nom::cfg_double;
        using ::nom::cfg_int;
        using ::nom::cfg_string;
        using ::nom::cfg_uint;

        using ::nom::cfg_defaulted;
        using ::nom::cfg_map;
        using ::nom::cfg_optional;
        using ::nom::cfg_unordered_map;
        using ::nom::cfg_vector;
    } // namespace cfg_types

    // Applications opt in their structures or custom scalar types explicitly.
    // Opt-in does not inspect members or supply a serialization mapping.
    template <typename Value> inline constexpr bool enable_config_type = false;

    namespace detail
    {
        template <typename Value> std::false_type nom_config_type(Value*);

        template <typename Value> constexpr bool custom_config_type_enabled()
        {
            if constexpr (std::is_class_v<Value> || std::is_enum_v<Value>)
            {
                // An adapter may instead declare this marker beside an application type.
                return enable_config_type<Value> || decltype(nom_config_type(static_cast<Value*>(nullptr)))::value;
            }
            return false;
        }
    } // namespace detail

    template <typename Value> inline constexpr bool is_config_value = detail::custom_config_type_enabled<Value>();
    template <> inline constexpr bool is_config_value<cfg_bool> = true;
    template <> inline constexpr bool is_config_value<cfg_string> = true;
    template <typename Value> inline constexpr bool is_config_value<detail::config_number<Value>> = true;
    template <typename Value> inline constexpr bool is_config_value<cfg_defaulted<Value>> = is_config_value<Value>;
    template <typename Value, typename Allocator>
    inline constexpr bool is_config_value<std::vector<Value, Allocator>> = is_config_value<Value>;
    template <typename Value, typename Compare, typename Allocator>
    inline constexpr bool is_config_value<std::map<std::string, Value, Compare, Allocator>> = is_config_value<Value>;
    template <typename Value, typename Hash, typename Equal, typename Allocator>
    inline constexpr bool is_config_value<std::unordered_map<std::string, Value, Hash, Equal, Allocator>> =
        is_config_value<Value>;

    // An optional is a structure member, not an array element or a map value.
    template <typename Value> inline constexpr bool is_config_member = is_config_value<Value>;
    template <typename Value> inline constexpr bool is_config_member<std::optional<Value>> = is_config_value<Value>;
} // namespace nom
