// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <json_config/config.h>

import std;

import nom.json_config;

namespace runtime_fixture
{
    using namespace nom::cfg_types;

    struct app_config
    {
        cfg_string label;
        cfg_int lower;
        cfg_int upper;
        cfg_optional<cfg_int> retries;
        cfg_defaulted<cfg_bool> active{true};

        inline static int validation_count{};

        std::expected<void, std::string> validate() const
        {
            ++validation_count;
            if (lower > upper)
            {
                return std::unexpected("lower must not exceed upper");
            }
            return {};
        }
    };

    NOM_JCONFIG(app_config, label, lower, upper, retries, active)

    struct hard_wired_config
    {
        cfg_defaulted<cfg_int> count{3};
        cfg_defaulted<cfg_string> label{"built-in"};
        cfg_optional<cfg_int> spare;

        std::expected<void, std::string> validate() const
        {
            if (*count < 0)
            {
                return std::unexpected("count must not be negative");
            }
            return {};
        }
    };

    NOM_JCONFIG(hard_wired_config, count, label, spare)

    struct required_config
    {
        cfg_int count{4};

        std::expected<void, std::string> validate() const
        {
            return {};
        }
    };

    NOM_JCONFIG(required_config, count)

    struct throwing_config
    {
        cfg_defaulted<cfg_bool> active{true};

        std::expected<void, std::string> validate() const
        {
            throw std::runtime_error("validation unavailable");
        }
    };

    NOM_JCONFIG(throwing_config, active)

    struct fixture
    {
        std::filesystem::path directory;
        nom::jcfg::config_options options;
        std::map<std::string, std::string, std::less<>> variables;

        fixture()
        {
            static std::atomic<unsigned> sequence{};
            const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            directory = std::filesystem::temp_directory_path() /
                        ("jcfg-runtime-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
            std::filesystem::create_directories(directory / "bin");
            options.executable_path = directory / "bin" / "sample";
            app_config::validation_count = 0;
        }

        ~fixture()
        {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }

        nom::jcfg::environment_lookup environment()
        {
            return [this](std::string_view name) -> std::optional<std::string>
            {
                if (const auto found = variables.find(name); found != variables.end())
                {
                    return found->second;
                }
                return std::nullopt;
            };
        }

        void write_config(std::string_view document)
        {
            options.config_file = directory / "selected.json";
            std::ofstream output(*options.config_file);
            output << document;
            REQUIRE(output.good());
        }
    };

    template <typename Action> nom::jcfg::config_error capture_error(Action&& action)
    {
        try
        {
            std::forward<Action>(action)();
        }
        catch (const nom::jcfg::config_error& error)
        {
            return error;
        }
        FAIL("Expected config_error");
        throw std::logic_error("Expected config_error");
    }
} // namespace runtime_fixture

using namespace runtime_fixture;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("typed loading returns validated config and its location", "[runtime]")
{
    fixture test;
    test.variables.emplace("LABEL", "capture");
    test.write_config(R"({"label":"{jcfg:run-var LABEL}","lower":1,"upper":5})");
    const auto loaded = nom::jcfg::load_config<app_config>(test.options, test.environment());
    CHECK(loaded.value.label == "capture");
    CHECK(loaded.value.lower == 1);
    CHECK(loaded.value.upper == 5);
    CHECK(*loaded.value.active);
    CHECK_FALSE(loaded.value.retries);
    CHECK(loaded.location.path == test.options.config_file);
    CHECK(loaded.location.source == nom::jcfg::config_source::command_line);
    CHECK(app_config::validation_count == 1);
}

TEST_CASE("runtime substitution precedes typed overrides and final validation", "[runtime]")
{
    fixture test;
    test.variables.emplace("LABEL", "from environment");
    test.write_config(R"({"label":"{jcfg:run-var LABEL}","lower":1,"upper":5,"retries":null,"active":true})");
    test.options.overrides = {
        {"/lower", "8"}, {"/upper", "9"}, {"/label", "123"}, {"/retries", "4"}, {"/upper", "10"}, {"/active", "false"}};
    const auto loaded = nom::jcfg::load_config<app_config>(test.options, test.environment());
    CHECK(loaded.value.lower == 8);
    CHECK(loaded.value.upper == 10);
    CHECK(loaded.value.label == "123");
    REQUIRE(loaded.value.retries);
    CHECK(*loaded.value.retries == 4);
    CHECK_FALSE(*loaded.value.active);
    CHECK(app_config::validation_count == 1);
}

TEST_CASE("missing runtime variable fails before an override can replace it", "[runtime]")
{
    fixture test;
    test.write_config(R"({"label":"{jcfg:run-var ABSENT}","lower":1,"upper":5})");
    test.options.overrides = {{"/label", "replacement"}};
    const auto error = capture_error([&] { nom::jcfg::load_config<app_config>(test.options, test.environment()); });
    CHECK(error.stage == nom::jcfg::config_stage::substitution);
    CHECK(error.file == test.options.config_file);
    CHECK(error.pointer == "/label");
    CHECK_THAT(error.what(), ContainsSubstring("ABSENT"));
    CHECK(app_config::validation_count == 0);
}

TEST_CASE("hard-wired config maps defaults when no file is found", "[runtime]")
{
    fixture test;
    const auto loaded = nom::jcfg::load_config<hard_wired_config>(test.options, test.environment());
    CHECK(*loaded.value.count == 3);
    CHECK(*loaded.value.label == "built-in");
    CHECK_FALSE(loaded.value.spare);
    CHECK_FALSE(loaded.location.path);
    CHECK(loaded.location.source == nom::jcfg::config_source::hard_wired);
}

TEST_CASE("hard-wired override targets come from serializing initialized config", "[runtime]")
{
    fixture test;
    test.options.overrides = {{"/count", "7"}, {"/label", "custom"}, {"/spare", "12"}};
    const auto loaded = nom::jcfg::load_config<hard_wired_config>(test.options, test.environment());
    CHECK(*loaded.value.count == 7);
    CHECK(*loaded.value.label == "custom");
    REQUIRE(loaded.value.spare);
    CHECK(*loaded.value.spare == 12);
    CHECK(loaded.location.source == nom::jcfg::config_source::hard_wired);
}

TEST_CASE("C++ initializers do not make required members optional", "[runtime]")
{
    fixture test;
    SECTION("without overrides")
    {
    }
    SECTION("override cannot bypass initial hard-wired mapping")
    {
        test.options.overrides = {{"/count", "9"}};
    }
    const auto error =
        capture_error([&] { nom::jcfg::load_config<required_config>(test.options, test.environment()); });
    CHECK(error.stage == nom::jcfg::config_stage::mapping);
    CHECK_FALSE(error.file);
    CHECK_THAT(error.what(), ContainsSubstring("count"));
}

TEST_CASE("final validation failures retain stage file and domain explanation", "[runtime]")
{
    fixture test;
    test.write_config(R"({"label":"capture","lower":1,"upper":5})");
    test.options.overrides = {{"/lower", "8"}};
    const auto error = capture_error([&] { nom::jcfg::load_config<app_config>(test.options, test.environment()); });
    CHECK(error.stage == nom::jcfg::config_stage::validation);
    CHECK(error.file == test.options.config_file);
    CHECK_THAT(error.what(), ContainsSubstring("lower must not exceed upper"));
    CHECK(app_config::validation_count == 1);
}

TEST_CASE("validation exceptions become config errors", "[runtime]")
{
    fixture test;
    const auto error =
        capture_error([&] { nom::jcfg::load_config<throwing_config>(test.options, test.environment()); });
    CHECK(error.stage == nom::jcfg::config_stage::validation);
    CHECK_FALSE(error.file);
    CHECK_THAT(error.what(), ContainsSubstring("validation unavailable"));
}

TEST_CASE("mapping errors identify the selected file offending path and value", "[runtime]")
{
    fixture test;
    test.write_config(R"({"label":"capture","lower":1,"upper":2.5})");
    const auto error = capture_error([&] { nom::jcfg::load_config<app_config>(test.options, test.environment()); });
    CHECK(error.stage == nom::jcfg::config_stage::mapping);
    CHECK(error.file == test.options.config_file);
    CHECK_THAT(error.what(), ContainsSubstring("/upper"));
    CHECK_THAT(error.what(), ContainsSubstring("2.5"));
    CHECK(app_config::validation_count == 0);
}

TEST_CASE("the first override can repair its own unmappable value", "[runtime]")
{
    fixture test;
    test.write_config(R"({"label":"capture","lower":"broken","upper":5})");
    test.options.overrides = {{"/lower", "2"}};
    const auto loaded = nom::jcfg::load_config<app_config>(test.options, test.environment());
    CHECK(loaded.value.lower == 2);
    CHECK(app_config::validation_count == 1);
}

TEST_CASE("later overrides cannot rescue a failed earlier mapping probe", "[runtime]")
{
    fixture test;
    test.write_config(R"({"label":"capture","lower":1,"upper":"broken"})");
    test.options.overrides = {{"/lower", "2"}, {"/upper", "5"}};
    const auto error = capture_error([&] { nom::jcfg::load_config<app_config>(test.options, test.environment()); });
    CHECK(error.stage == nom::jcfg::config_stage::overrides);
    CHECK(error.pointer == "/lower");
    CHECK(app_config::validation_count == 0);
}

TEST_CASE("file errors do not fall back to hard-wired config", "[runtime]")
{
    fixture test;
    SECTION("missing explicit file")
    {
        test.options.config_file = test.directory / "missing.json";
        const auto error =
            capture_error([&] { nom::jcfg::load_config<hard_wired_config>(test.options, test.environment()); });
        CHECK(error.stage == nom::jcfg::config_stage::location);
    }
    SECTION("malformed selected file")
    {
        test.write_config("{");
        const auto error =
            capture_error([&] { nom::jcfg::load_config<hard_wired_config>(test.options, test.environment()); });
        CHECK(error.stage == nom::jcfg::config_stage::parsing);
    }
}

TEST_CASE("CLI parsing keeps application arguments and consumes only config options", "[runtime][arguments]")
{
    const std::array<std::string_view, 12> arguments{"--verbose",
                                                     "input",
                                                     "--config",
                                                     "settings.json",
                                                     "--set",
                                                     "/name=a=b",
                                                     "--check-config",
                                                     "--set",
                                                     "/name=",
                                                     "--",
                                                     "--config",
                                                     "application-owned"};
    const auto parsed = nom::jcfg::parse_config_arguments(arguments, NOM_CONFIG_KEY_VALUE);
    CHECK(parsed.options.config_key == "runtime_config");
    CHECK(parsed.options.config_file == "settings.json");
    CHECK(parsed.options.check_config);
    REQUIRE(parsed.options.overrides.size() == 2);
    CHECK(parsed.options.overrides[0].pointer == "/name");
    CHECK(parsed.options.overrides[0].value == "a=b");
    CHECK(parsed.options.overrides[1].pointer == "/name");
    CHECK(parsed.options.overrides[1].value.empty());
    CHECK(parsed.remaining_arguments ==
          std::vector<std::string>{"--verbose", "input", "--", "--config", "application-owned"});
    CHECK(parsed.options.executable_path.empty());
}

TEST_CASE("argc adapter excludes argv zero and preserves root pointer overrides", "[runtime][arguments]")
{
    const char* arguments[] = {"app", "--set", "={\"count\":3}", "other"};
    const auto parsed = nom::jcfg::parse_config_arguments(4, arguments);
    REQUIRE(parsed.options.overrides.size() == 1);
    CHECK(parsed.options.overrides.front().pointer.empty());
    CHECK(parsed.options.overrides.front().value == R"({"count":3})");
    CHECK(parsed.remaining_arguments == std::vector<std::string>{"other"});
    CHECK(parsed.options.config_key.empty());
    CHECK_FALSE(parsed.options.check_config);
    CHECK_FALSE(parsed.options.config_file);
    CHECK(nom::jcfg::parse_config_arguments(0, nullptr).remaining_arguments.empty());
}

TEST_CASE("CLI parsing accepts adjacent long-option values and preserves override order", "[runtime][arguments]")
{
    const std::array<std::string_view, 6> arguments{"--config=settings=dev.json",
                                                    "--set=/name=first=value",
                                                    "--set",
                                                    "/name=second",
                                                    "--set=/name=",
                                                    "--check-config"};
    const auto parsed = nom::jcfg::parse_config_arguments(arguments);
    CHECK(parsed.options.config_file == "settings=dev.json");
    CHECK(parsed.options.check_config);
    REQUIRE(parsed.options.overrides.size() == 3);
    CHECK(parsed.options.overrides[0].pointer == "/name");
    CHECK(parsed.options.overrides[0].value == "first=value");
    CHECK(parsed.options.overrides[1].pointer == "/name");
    CHECK(parsed.options.overrides[1].value == "second");
    CHECK(parsed.options.overrides[2].pointer == "/name");
    CHECK(parsed.options.overrides[2].value.empty());
    CHECK(parsed.remaining_arguments.empty());
}

TEST_CASE("CLI parsing preserves unrelated options without guessing config names", "[runtime][arguments]")
{
    const std::array<std::string_view, 16> arguments{"--conf",
                                                     "app.json",
                                                     "--configuration=local",
                                                     "--set-extra=/name=app",
                                                     "--check",
                                                     "-v",
                                                     "-abc",
                                                     "-xvalue",
                                                     "-m=mode",
                                                     "--app=value",
                                                     "--label=",
                                                     "--unknown",
                                                     "--=opaque",
                                                     "input",
                                                     "--check-config",
                                                     "--check-config"};
    const auto parsed = nom::jcfg::parse_config_arguments(arguments);
    CHECK_FALSE(parsed.options.config_file);
    CHECK(parsed.options.overrides.empty());
    CHECK(parsed.options.check_config);
    CHECK(parsed.remaining_arguments == std::vector<std::string>{"--conf",
                                                                 "app.json",
                                                                 "--configuration=local",
                                                                 "--set-extra=/name=app",
                                                                 "--check",
                                                                 "-v",
                                                                 "-abc",
                                                                 "-xvalue",
                                                                 "-m=mode",
                                                                 "--app=value",
                                                                 "--label=",
                                                                 "--unknown",
                                                                 "--=opaque",
                                                                 "input"});
}

TEST_CASE("CLI terminator preserves subsequent config-like tokens verbatim", "[runtime][arguments]")
{
    const std::array<std::string_view, 5> arguments{
        "--", "--config=app.json", "--set=/name=value", "--check-config", "--"};
    const auto parsed = nom::jcfg::parse_config_arguments(arguments);
    CHECK_FALSE(parsed.options.config_file);
    CHECK_FALSE(parsed.options.check_config);
    CHECK(parsed.options.overrides.empty());
    CHECK(parsed.remaining_arguments ==
          std::vector<std::string>{"--", "--config=app.json", "--set=/name=value", "--check-config", "--"});
}

TEST_CASE("malformed config command-line options fail during argument parsing", "[runtime][arguments]")
{
    std::vector<std::string_view> arguments;
    SECTION("missing config filename")
    {
        arguments = {"--config"};
    }
    SECTION("empty config filename")
    {
        arguments = {"--config", ""};
    }
    SECTION("empty adjacent config filename")
    {
        arguments = {"--config="};
    }
    SECTION("terminator is not a config filename")
    {
        arguments = {"--config", "--"};
    }
    SECTION("check option is not a config filename")
    {
        arguments = {"--config", "--check-config"};
    }
    SECTION("override option is not a config filename")
    {
        arguments = {"--config", "--set"};
    }
    SECTION("config option is not a config filename")
    {
        arguments = {"--config", "--config"};
    }
    SECTION("duplicate config selection")
    {
        arguments = {"--config", "one", "--config", "two"};
    }
    SECTION("duplicate mixed-form config selection")
    {
        arguments = {"--config=one", "--config", "two"};
    }
    SECTION("missing override")
    {
        arguments = {"--set"};
    }
    SECTION("terminator is not an override")
    {
        arguments = {"--set", "--"};
    }
    SECTION("override without equals")
    {
        arguments = {"--set", "/key"};
    }
    SECTION("adjacent override without equals")
    {
        arguments = {"--set=/key"};
    }
    SECTION("check flag does not accept an argument")
    {
        arguments = {"--check-config=anything"};
    }
    const auto error = capture_error([&] { nom::jcfg::parse_config_arguments(arguments); });
    CHECK(error.stage == nom::jcfg::config_stage::arguments);
    CHECK_FALSE(error.file);
}

TEST_CASE("option names remain usable inside explicit paths and override values", "[runtime][arguments]")
{
    const std::array<std::string_view, 4> arguments{"--config", "./--check-config", "--set", "/key=--check-config"};
    const auto parsed = nom::jcfg::parse_config_arguments(arguments);
    CHECK(parsed.options.config_file == "./--check-config");
    REQUIRE(parsed.options.overrides.size() == 1);
    CHECK(parsed.options.overrides.front().pointer == "/key");
    CHECK(parsed.options.overrides.front().value == "--check-config");
    CHECK_FALSE(parsed.options.check_config);
}

TEST_CASE("process environment lookup rejects invalid names without truncation", "[runtime][environment]")
{
    CHECK_FALSE(nom::jcfg::process_environment(""));
    CHECK_FALSE(nom::jcfg::process_environment("NAME=value"));
    constexpr char null_name[] = "PATH\0suffix";
    CHECK_FALSE(nom::jcfg::process_environment(std::string_view(null_name, sizeof(null_name) - 1)));
}

TEST_CASE("argc adapter rejects invalid arrays", "[runtime][arguments]")
{
    CHECK(capture_error([] { nom::jcfg::parse_config_arguments(-1, nullptr); }).stage ==
          nom::jcfg::config_stage::arguments);
    CHECK(capture_error([] { nom::jcfg::parse_config_arguments(1, nullptr); }).stage ==
          nom::jcfg::config_stage::arguments);
    const char* arguments[] = {"app", nullptr};
    CHECK(capture_error([&] { nom::jcfg::parse_config_arguments(2, arguments); }).stage ==
          nom::jcfg::config_stage::arguments);
}
