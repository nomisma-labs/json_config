// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <json_config/config.h>

import std;

import nom.json_config;

namespace enum_mapping_fixture
{
    using namespace nom::cfg_types;

    enum class severity
    {
        debug = -7,
        info = 4,
        warn = 9,
        unsupported = 12
    };

    NOM_JCONFIG_ENUM(
        severity,
        {{severity::debug, "debug"}, {severity::info, "info"}, {severity::warn, "warn"}, {severity::info, "INFO"}})

    struct log_config
    {
        cfg_defaulted<severity> level{severity::info};
    };

    NOM_JCONFIG(log_config, level)

    struct app_config
    {
        severity required{severity::info};
        cfg_defaulted<log_config> log;
        cfg_optional<severity> optional{severity::warn};
        cfg_defaulted<cfg_vector<severity>> levels;
        cfg_defaulted<cfg_map<severity>> named;
    };

    NOM_JCONFIG(app_config, required, log, optional, levels, named)

    static_assert(nom::is_config_member<severity>);
    static_assert(nom::is_config_member<cfg_defaulted<severity>>);
    static_assert(nom::is_config_member<cfg_optional<severity>>);
    static_assert(nom::is_config_member<cfg_vector<severity>>);
    static_assert(nom::is_config_member<cfg_map<severity>>);
} // namespace enum_mapping_fixture

TEST_CASE("enum mappings round trip explicit values and canonicalize aliases", "[mapping][enum]")
{
    using enum_mapping_fixture::severity;
    using nlohmann::json;

    for (const auto& [value, name] :
         {std::pair{severity::debug, "debug"}, std::pair{severity::info, "info"}, std::pair{severity::warn, "warn"}})
    {
        CAPTURE(name);
        const json encoded = value;
        REQUIRE(encoded == name);
        REQUIRE(encoded.get<severity>() == value);
    }

    const auto alias = json("INFO").get<severity>();
    REQUIRE(alias == severity::info);
    REQUIRE(json(alias) == "info");
}

TEST_CASE("enum mappings reject unknown names and every non-string JSON shape", "[mapping][enum]")
{
    using enum_mapping_fixture::severity;
    using nlohmann::json;

    for (const json& document : {json("unknown"),
                                 json("Info"),
                                 json(""),
                                 json(4),
                                 json(4.0),
                                 json(true),
                                 json(nullptr),
                                 json::array({"info"}),
                                 json{{"level", "info"}}})
    {
        CAPTURE(document.dump());
        auto value = severity::warn;
        REQUIRE_THROWS_AS(document.get_to(value), json::type_error);
        REQUIRE(value == severity::warn);
    }
}

TEST_CASE("enum serialization rejects unmapped named and unnamed values", "[mapping][enum]")
{
    using Catch::Matchers::ContainsSubstring;
    using enum_mapping_fixture::severity;
    using nlohmann::json;

    for (const auto value : {severity::unsupported, static_cast<severity>(100)})
    {
        json destination = "unchanged";
        REQUIRE_THROWS_WITH(enum_mapping_fixture::to_json(destination, value),
                            ContainsSubstring("severity") &&
                                ContainsSubstring(std::to_string(std::to_underlying(value))));
        REQUIRE(destination == "unchanged");
        REQUIRE_THROWS_AS(json(value), json::type_error);
    }
}

TEST_CASE("enum fields preserve required defaulted and optional presence policies", "[mapping][enum]")
{
    using namespace enum_mapping_fixture;
    using nlohmann::json;

    REQUIRE_THROWS_AS(json::object().get<app_config>(), json::out_of_range);
    REQUIRE_THROWS_AS(json({{"required", nullptr}}).get<app_config>(), json::type_error);

    const auto omitted = json{{"required", "debug"}}.get<app_config>();
    REQUIRE(omitted.required == severity::debug);
    REQUIRE(*omitted.log->level == severity::info);
    REQUIRE(omitted.optional == severity::warn);

    const auto cleared =
        json{{"required", "info"}, {"log", {{"level", nullptr}}}, {"optional", nullptr}}.get<app_config>();
    REQUIRE(*cleared.log->level == severity::info);
    REQUIRE_FALSE(cleared.optional);

    app_config existing;
    existing.log->level = {severity::warn};
    json{{"required", "info"}, {"log", json::object()}}.get_to(existing);
    REQUIRE(*existing.log->level == severity::warn);
    json{{"required", "info"}, {"log", {{"level", nullptr}}}}.get_to(existing);
    REQUIRE(*existing.log->level == severity::warn);
    json{{"required", "info"}, {"log", {{"level", "debug"}}}, {"optional", "info"}}.get_to(existing);
    REQUIRE(*existing.log->level == severity::debug);
    REQUIRE(existing.optional == severity::info);
}

TEST_CASE("enum mappings compose with structures and containers", "[mapping][enum]")
{
    using namespace enum_mapping_fixture;
    using nlohmann::json;

    const json document{{"required", "warn"},
                        {"log", {{"level", "info"}}},
                        {"optional", "debug"},
                        {"levels", {"info", "debug"}},
                        {"named", {{"network", "warn"}}}};
    const auto config = document.get<app_config>();
    REQUIRE(config.levels->at(0) == severity::info);
    REQUIRE(config.levels->at(1) == severity::debug);
    REQUIRE(config.named->at("network") == severity::warn);
    REQUIRE(json(config) == document);
    REQUIRE(json(config).get<app_config>().required == severity::warn);
}

TEST_CASE("invalid enums in wrappers and containers identify their JSON path and value", "[mapping][enum]")
{
    using Catch::Matchers::ContainsSubstring;
    using enum_mapping_fixture::app_config;
    using nlohmann::json;

    for (const auto& [field, value, path] : {std::tuple{"required", json("typo"), "/required"},
                                             std::tuple{"log", json{{"level", "typo"}}, "/log/level"},
                                             std::tuple{"optional", json("typo"), "/optional"},
                                             std::tuple{"levels", json::array({"info", "typo"}), "/levels/1"},
                                             std::tuple{"named", json{{"a/b~c", "typo"}}, "/named/a~1b~0c"}})
    {
        json document{{"required", "info"}};
        document[field] = value;
        CAPTURE(path);
        REQUIRE_THROWS_WITH(document.get<app_config>(),
                            ContainsSubstring(path) && ContainsSubstring("severity") && ContainsSubstring("typo"));
    }
}
