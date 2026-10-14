// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <json_config/config.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

import nom.json_config;

namespace loader_fixture
{
    using nlohmann::json;
    using namespace nom::cfg_types;

    struct override_config
    {
        cfg_string name;
        cfg_int count;
        cfg_bool enabled;
        cfg_optional<cfg_int> limit;
        cfg_vector<cfg_int> items;
    };

    NOM_JCONFIG(override_config, name, count, enabled, limit, items)

    struct temporary_directory
    {
        std::filesystem::path path;

        temporary_directory()
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned int attempt = 0; attempt < 100; ++attempt)
            {
                const auto candidate = std::filesystem::temp_directory_path() /
                                       ("jcfg_loader_" + std::to_string(stamp) + '_' + std::to_string(attempt));
                if (std::filesystem::create_directory(candidate))
                {
                    path = candidate;
                    return;
                }
            }
            throw std::runtime_error("cannot create temporary test directory");
        }

        ~temporary_directory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }

        std::filesystem::path write(std::string_view name, std::string_view contents) const
        {
            const auto file = path / name;
            std::ofstream output(file, std::ios::binary);
            output << contents;
            output.close();
            if (!output)
            {
                throw std::runtime_error("cannot write test config");
            }
            return file;
        }
    };

    const nom::jcfg::mapping_probe probe = [](const json& document)
    { static_cast<void>(document.get<override_config>()); };

    json initial_config()
    {
        return json{{"name", "worker"}, {"count", 1}, {"enabled", false}, {"limit", nullptr}, {"items", {1, 2}}};
    }
} // namespace loader_fixture

TEST_CASE("JSON reading requires an object and identifies parse failures", "[loader]")
{
    using namespace nom::jcfg;
    loader_fixture::temporary_directory directory;
    const auto valid_file = directory.write("valid.json", R"({"count":2,"name":"worker"})");
    REQUIRE(read_config({valid_file, config_source::command_line}) ==
            loader_fixture::json{{"count", 2}, {"name", "worker"}});
    REQUIRE(read_config({}) == loader_fixture::json::object());

    for (const auto& text : {"", "{", "[]", "null", "123", "{} {}", "{\"count\":2,}", "{\"count\":1e400}"})
    {
        const auto invalid_file = directory.write("invalid.json", text);
        INFO(text);
        try
        {
            static_cast<void>(read_config({invalid_file, config_source::command_line}));
            FAIL("invalid object was accepted");
        }
        catch (const config_error& error)
        {
            REQUIRE(error.stage == config_stage::parsing);
            REQUIRE(error.file == invalid_file);
            REQUIRE_THAT(error.what(), Catch::Matchers::ContainsSubstring("invalid.json"));
        }
    }
}

TEST_CASE("JSON reading reports missing and unusable files", "[loader]")
{
    using namespace nom::jcfg;
    loader_fixture::temporary_directory directory;
    for (const auto& file : {directory.path / "missing.json", directory.path})
    {
        try
        {
            static_cast<void>(read_config({file, config_source::command_line}));
            FAIL("unusable file was accepted");
        }
        catch (const config_error& error)
        {
            REQUIRE(error.stage == config_stage::reading);
            REQUIRE(error.file == file);
        }
    }
    REQUIRE_THROWS_AS(read_config({std::nullopt, config_source::command_line}), config_error);

    const auto valid_file = directory.write("valid.json", "{}");
    auto truncated_path = valid_file.string();
    truncated_path += '\0';
    truncated_path += "ignored.json";
    REQUIRE_THROWS_AS(read_config({std::filesystem::path(truncated_path), config_source::command_line}), config_error);
}

TEST_CASE("Runtime substitution visits values once without changing their JSON type", "[loader]")
{
    using namespace nom::jcfg;
    using loader_fixture::json;
    const std::map<std::string, std::string, std::less<>> values{
        {"ROOT", "/srv"}, {"COUNT", "123"}, {"EMPTY", ""}, {"LITERAL", "{jcfg:run-var MISSING}"}};
    std::vector<std::string> looked_up;
    const environment_lookup environment = [&](std::string_view name) -> std::optional<std::string>
    {
        looked_up.emplace_back(name);
        if (const auto found = values.find(name); found != values.end())
        {
            return found->second;
        }
        return {};
    };
    json document{
        {"path", "{jcfg:run-var ROOT}/logs/{jcfg:run-var COUNT}"},
        {"array",
         json::array({"{jcfg:run-var COUNT}", "{jcfg:run-var EMPTY}", "{jcfg:run-var LITERAL}", 42, true, nullptr})},
        {"{jcfg:run-var KEY}", "unchanged"},
        {"generation", "jcfg:import donor.json"},
        {"generation_variable", "{jcfg:gen-var ROOT}"}};
    substitute_runtime_variables(document, environment);
    REQUIRE(document.at("path") == "/srv/logs/123");
    REQUIRE(document.at("array") == json::array({"123", "", "{jcfg:run-var MISSING}", 42, true, nullptr}));
    REQUIRE(document.at("{jcfg:run-var KEY}") == "unchanged");
    REQUIRE(document.at("generation") == "jcfg:import donor.json");
    REQUIRE(document.at("generation_variable") == "{jcfg:gen-var ROOT}");
    REQUIRE(looked_up.size() == 5);
}

TEST_CASE("Runtime substitution supports whitespace and escaped pointer diagnostics", "[loader]")
{
    using namespace nom::jcfg;
    using loader_fixture::json;
    const environment_lookup missing = [](std::string_view) -> std::optional<std::string> { return {}; };
    const config_location location{std::filesystem::path("app.json"), config_source::command_line};
    json document{{"a/b~c", json::array({"{jcfg:run-var\t MISSING}"})}};
    try
    {
        substitute_runtime_variables(document, missing, location);
        FAIL("missing environment variable was accepted");
    }
    catch (const config_error& error)
    {
        REQUIRE(error.stage == config_stage::substitution);
        REQUIRE(error.file == location.path);
        REQUIRE(error.pointer == "/a~1b~0c/0");
        REQUIRE_THAT(error.what(), Catch::Matchers::ContainsSubstring("MISSING"));
    }
}

TEST_CASE("Malformed runtime placeholders fail without interpreting generation instructions", "[loader]")
{
    using namespace nom::jcfg;
    for (const auto& text :
         {"{jcfg:run-var}", "{jcfg:run-var }", "{jcfg:run-var NAME", "{jcfg:run-var NAME }", "{jcfg:run-varNAME}"})
    {
        loader_fixture::json document{{"value", text}};
        INFO(text);
        REQUIRE_THROWS_AS(substitute_runtime_variables(document), config_error);
    }
}

TEST_CASE("Overrides select native mapped types and preserve text for strings", "[loader]")
{
    using namespace nom::jcfg;
    using loader_fixture::json;
    auto document = loader_fixture::initial_config();
    const std::vector<config_override> overrides{
        {"/name", "123"}, {"/count", "42"}, {"/enabled", "true"}, {"/limit", "9"}, {"/items", "[3,4]"}};
    apply_config_overrides(document, overrides, loader_fixture::probe);
    REQUIRE(document == json{{"name", "123"}, {"count", 42}, {"enabled", true}, {"limit", 9}, {"items", {3, 4}}});

    const std::vector<config_override> more{{"/limit", "null"}, {"/name", ""}, {"/count", "43"}, {"/count", "44"}};
    apply_config_overrides(document, more, loader_fixture::probe);
    REQUIRE(document.at("limit").is_null());
    REQUIRE(document.at("name") == "");
    REQUIRE(document.at("count") == 44);
}

TEST_CASE("An override can repair its own invalid value before mapping", "[loader]")
{
    using namespace nom::jcfg;
    auto document = loader_fixture::initial_config();
    document["count"] = "invalid";
    const std::vector<config_override> overrides{{"/count", "7"}};
    apply_config_overrides(document, overrides, loader_fixture::probe);
    REQUIRE(document.at("count") == 7);
}

TEST_CASE("Overrides support escaped pointers array elements and the root", "[loader]")
{
    using namespace nom::jcfg;
    using loader_fixture::json;
    json document{{"a/b~c", json::array({1, 2})}};
    const mapping_probe array_probe = [](const json& candidate)
    { static_cast<void>(candidate.at("a/b~c").get<nom::cfg_vector<nom::cfg_int>>()); };
    const std::vector<config_override> overrides{{"/a~1b~0c/1", "3"}};
    apply_config_overrides(document, overrides, array_probe);
    REQUIRE(document == json{{"a/b~c", {1, 3}}});

    const std::vector<config_override> root{{"", R"({"a/b~c":[4]})"}};
    apply_config_overrides(document, root, array_probe);
    REQUIRE(document == json{{"a/b~c", {4}}});
}

TEST_CASE("Invalid override pointers do not create targets or invoke mapping", "[loader]")
{
    using namespace nom::jcfg;
    const config_location location{std::filesystem::path("app.json"), config_source::command_line};
    for (const auto& pointer : {"name", "/missing", "/items/2", "/items/-", "/items/01", "/items/~2", "/name/child"})
    {
        auto document = loader_fixture::initial_config();
        const auto original = document;
        bool mapped = false;
        const mapping_probe probe = [&](const loader_fixture::json&) { mapped = true; };
        const std::vector<config_override> overrides{{pointer, "1"}};
        INFO(pointer);
        try
        {
            apply_config_overrides(document, overrides, probe, location);
            FAIL("missing or malformed target was accepted");
        }
        catch (const config_error& error)
        {
            REQUIRE(error.stage == config_stage::overrides);
            REQUIRE(error.pointer == pointer);
            REQUIRE(error.file == location.path);
        }
        REQUIRE_FALSE(mapped);
        REQUIRE(document == original);
    }
}

TEST_CASE("Overrides fail sequentially and do not publish a failed candidate", "[loader]")
{
    using namespace nom::jcfg;
    for (const auto& text : {"1.5", "true", "bad JSON", "1e400"})
    {
        auto document = loader_fixture::initial_config();
        const std::vector<config_override> overrides{{"/name", "first"}, {"/count", text}, {"/name", "last"}};
        INFO(text);
        REQUIRE_THROWS_AS(apply_config_overrides(document, overrides, loader_fixture::probe), config_error);
        REQUIRE(document.at("name") == "first");
        REQUIRE(document.at("count") == 1);
    }
}

TEST_CASE("Memory allocation failures are not treated as mapping type mismatches", "[loader]")
{
    using namespace nom::jcfg;
    auto document = loader_fixture::initial_config();
    const auto original = document;
    unsigned int attempts = 0;
    const mapping_probe probe = [&](const loader_fixture::json&)
    {
        ++attempts;
        throw std::bad_alloc();
    };
    const std::vector<config_override> overrides{{"/count", "2"}};
    REQUIRE_THROWS_AS(apply_config_overrides(document, overrides, probe), std::bad_alloc);
    REQUIRE(attempts == 1);
    REQUIRE(document == original);
}
