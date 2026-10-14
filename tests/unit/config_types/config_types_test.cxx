// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

import nom.config_types;

namespace config_type_tests
{
    struct connection
    {
        nom::cfg_string host{"localhost"};
        nom::cfg_uint port{9000};
    };

    struct unregistered
    {
        nom::cfg_int value;
    };

    struct marked
    {
        nom::cfg_string name;
    };

    std::true_type nom_config_type(marked*);

    enum class mode
    {
        disabled,
        enabled
    };
} // namespace config_type_tests

template <> inline constexpr bool nom::enable_config_type<config_type_tests::connection> = true;
template <> inline constexpr bool nom::enable_config_type<config_type_tests::mode> = true;

static_assert(std::same_as<nom::cfg_bool, bool>);
static_assert(std::same_as<nom::cfg_string, std::string>);
static_assert(std::same_as<nom::cfg_vector<nom::cfg_int>, std::vector<nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_map<nom::cfg_int>, std::map<std::string, nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_unordered_map<nom::cfg_int>, std::unordered_map<std::string, nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_optional<nom::cfg_int>, std::optional<nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_types::cfg_int, nom::cfg_int>);
static_assert(std::same_as<nom::cfg_types::cfg_uint, nom::cfg_uint>);
static_assert(std::same_as<nom::cfg_types::cfg_double, nom::cfg_double>);
static_assert(std::same_as<nom::cfg_types::cfg_bool, nom::cfg_bool>);
static_assert(std::same_as<nom::cfg_types::cfg_string, nom::cfg_string>);
static_assert(std::same_as<nom::cfg_types::cfg_vector<nom::cfg_int>, nom::cfg_vector<nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_types::cfg_map<nom::cfg_int>, nom::cfg_map<nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_types::cfg_unordered_map<nom::cfg_int>, nom::cfg_unordered_map<nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_types::cfg_optional<nom::cfg_int>, nom::cfg_optional<nom::cfg_int>>);
static_assert(std::same_as<nom::cfg_types::cfg_defaulted<nom::cfg_int>, nom::cfg_defaulted<nom::cfg_int>>);
static_assert(std::is_aggregate_v<nom::cfg_int>);
static_assert(std::is_aggregate_v<nom::cfg_uint>);
static_assert(std::is_aggregate_v<nom::cfg_double>);
static_assert(std::is_aggregate_v<nom::cfg_defaulted<config_type_tests::connection>>);
static_assert(std::is_trivially_copyable_v<nom::cfg_int>);
static_assert(sizeof(nom::cfg_int) == sizeof(std::int64_t));
static_assert(sizeof(nom::cfg_uint) == sizeof(std::uint64_t));
static_assert(sizeof(nom::cfg_double) == sizeof(double));
static_assert(std::same_as<decltype(std::declval<nom::cfg_int&>().get()), std::int64_t&>);
static_assert(std::same_as<decltype(std::declval<const nom::cfg_int&>().get()), const std::int64_t&>);
static_assert(std::same_as<decltype(std::declval<nom::cfg_uint&>().get()), std::uint64_t&>);
static_assert(std::same_as<decltype(std::declval<const nom::cfg_uint&>().get()), const std::uint64_t&>);
static_assert(std::same_as<decltype(std::declval<nom::cfg_double&>().get()), double&>);
static_assert(std::same_as<decltype(std::declval<const nom::cfg_double&>().get()), const double&>);

static_assert(nom::is_config_value<nom::cfg_int>);
static_assert(nom::is_config_value<nom::cfg_uint>);
static_assert(nom::is_config_value<nom::cfg_double>);
static_assert(nom::is_config_value<nom::cfg_bool>);
static_assert(nom::is_config_value<nom::cfg_string>);
static_assert(nom::is_config_value<config_type_tests::connection>);
static_assert(nom::is_config_value<config_type_tests::mode>);
static_assert(nom::is_config_value<config_type_tests::marked>);
static_assert(nom::is_config_value<nom::cfg_defaulted<config_type_tests::connection>>);
static_assert(nom::is_config_value<nom::cfg_vector<nom::cfg_map<config_type_tests::connection>>>);
static_assert(nom::is_config_value<nom::cfg_unordered_map<nom::cfg_vector<nom::cfg_double>>>);
static_assert(nom::is_config_member<nom::cfg_optional<config_type_tests::connection>>);
static_assert(nom::is_config_member<nom::cfg_optional<nom::cfg_vector<nom::cfg_int>>>);
static_assert(nom::is_config_member<nom::cfg_optional<nom::cfg_defaulted<nom::cfg_int>>>);
static_assert(!nom::is_config_value<int>);
static_assert(!nom::is_config_value<std::int64_t>);
static_assert(!nom::is_config_value<std::uint64_t>);
static_assert(!nom::is_config_value<float>);
static_assert(!nom::is_config_value<double>);
static_assert(!nom::is_config_value<void>);
static_assert(!nom::is_config_value<nom::cfg_int*>);
static_assert(!nom::is_config_value<nom::cfg_int&>);
static_assert(!nom::is_config_value<const char*>);
static_assert(!nom::is_config_value<config_type_tests::unregistered>);
static_assert(!nom::is_config_value<std::map<int, nom::cfg_int>>);
static_assert(!nom::is_config_value<std::unordered_map<int, nom::cfg_int>>);
static_assert(!nom::is_config_value<nom::cfg_optional<nom::cfg_int>>);
static_assert(!nom::is_config_member<nom::cfg_optional<int>>);
static_assert(!nom::is_config_member<nom::cfg_optional<nom::cfg_optional<nom::cfg_int>>>);
static_assert(!nom::is_config_member<nom::cfg_defaulted<nom::cfg_optional<nom::cfg_int>>>);
static_assert(!nom::is_config_value<nom::cfg_vector<nom::cfg_optional<nom::cfg_int>>>);
static_assert(!nom::is_config_value<nom::cfg_map<nom::cfg_optional<nom::cfg_int>>>);
static_assert(!nom::is_config_value<nom::cfg_unordered_map<nom::cfg_optional<nom::cfg_int>>>);
static_assert(!nom::is_config_value<nom::cfg_vector<nom::cfg_map<int>>>);

TEST_CASE("config numbers preserve their underlying values and mutable access", "[config_types]")
{
    nom::cfg_int signed_number;
    nom::cfg_uint unsigned_number;
    nom::cfg_double real_number;
    CHECK(signed_number == 0);
    CHECK(unsigned_number == 0);
    CHECK(real_number == 0.0);

    signed_number.get() = std::numeric_limits<std::int64_t>::min();
    unsigned_number.get() = std::numeric_limits<std::uint64_t>::max();
    real_number.get() = 1.25;
    const nom::cfg_int& signed_view = signed_number;
    const nom::cfg_uint& unsigned_view = unsigned_number;
    const nom::cfg_double& real_view = real_number;
    const std::int64_t signed_value = signed_view;
    const std::uint64_t unsigned_value = unsigned_view;
    const double real_value = real_view;

    CHECK(signed_value == std::numeric_limits<std::int64_t>::min());
    CHECK(unsigned_value == std::numeric_limits<std::uint64_t>::max());
    CHECK(real_value == 1.25);
    CHECK(&signed_view.get() == &signed_number.value);
    CHECK(&unsigned_view.get() == &unsigned_number.value);
    CHECK(&real_view.get() == &real_number.value);

    const nom::cfg_int designated{.value = 42};
    constexpr nom::cfg_double constant{2.5};
    static_assert(constant.get() == 2.5);
    CHECK(designated == 42);
    CHECK(constant + 0.5 == 3.0);
}

TEST_CASE("defaulted config preserves aggregate defaults and reference access", "[config_types]")
{
    nom::cfg_defaulted<config_type_tests::connection> settings;
    CHECK(settings->host == "localhost");
    CHECK(settings->port == 9000);

    config_type_tests::connection& reference = settings;
    CHECK(&reference == &settings.value);
    CHECK(&settings.get() == &settings.value);
    CHECK(&*settings == &settings.value);
    CHECK(settings.operator->() == &settings.value);
    reference.host = "example.test";
    settings->port = nom::cfg_uint{443};
    CHECK((*settings).host == "example.test");
    CHECK(settings.get().port == 443);

    const nom::cfg_defaulted<config_type_tests::connection>& view = settings;
    const config_type_tests::connection& const_reference = view;
    CHECK(&const_reference == &settings.value);
    CHECK(&view.get() == &settings.value);
    CHECK(&*view == &settings.value);
    CHECK(view.operator->() == &settings.value);
    CHECK(view->host == "example.test");
    CHECK((*view).port == 443);

    const nom::cfg_defaulted<config_type_tests::connection> explicit_defaults{{"remote", {8080}}};
    CHECK(explicit_defaults->host == "remote");
    CHECK(explicit_defaults->port == 8080);
    constexpr nom::cfg_defaulted<nom::cfg_int> constant{{7}};
    static_assert(constant->get() == 7);
}
