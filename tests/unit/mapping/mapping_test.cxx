// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <json_config/config.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

import nom.json_config;

namespace mapping_fixture
{
    using nlohmann::json;
    using namespace nom::cfg_types;

    struct empty_config
    {
    };

    NOM_JCONFIG(empty_config)

    struct endpoint
    {
        cfg_string address{"localhost"};
        cfg_defaulted<cfg_int> timeout_s{5};
        cfg_optional<cfg_string> interface_name;
    };

    NOM_JCONFIG(endpoint, address, timeout_s, interface_name)

    struct app_config
    {
        cfg_int workers{1};
        cfg_defaulted<endpoint> connection{endpoint{"parent", {42}, "eth0"}};
        cfg_optional<endpoint> standby{endpoint{"backup", {43}, "eth1"}};
        cfg_optional<cfg_string> note;
    };

    NOM_JCONFIG(app_config, workers, connection, standby, note)

    struct port_config
    {
        cfg_int port;

        std::expected<void, std::string> validate() const
        {
            if (port.get() <= 0 || port.get() > 65535)
            {
                return std::unexpected("port must be between 1 and 65535");
            }
            return {};
        }
    };

    NOM_JCONFIG(port_config, port)

    struct numeric_config
    {
        cfg_uint count;
        cfg_double rate;
        cfg_bool active;
        cfg_optional<cfg_int> limit;
    };

    NOM_JCONFIG(numeric_config, count, rate, active, limit)

    struct delay
    {
        std::chrono::milliseconds value;
    };

    template <typename Json> void from_json(const Json& document, delay& value)
    {
        value.value = std::chrono::milliseconds{document.template get<cfg_int>().get()};
    }

    template <typename Json> void to_json(Json& document, const delay& value)
    {
        document = value.value.count();
    }

    struct renamed_config
    {
        cfg_int connection_limit;
    };

    void from_json(const json& document, renamed_config& value)
    {
        document.at("connection-limit").get_to(value.connection_limit);
    }

    void to_json(json& document, const renamed_config& value)
    {
        document = json{{"connection-limit", value.connection_limit}};
    }

} // namespace mapping_fixture

template <> inline constexpr bool nom::enable_config_type<mapping_fixture::delay> = true;
template <> inline constexpr bool nom::enable_config_type<mapping_fixture::renamed_config> = true;

namespace mapping_fixture
{
    struct adapted_config
    {
        delay timeout;
        renamed_config limits;
    };

    NOM_JCONFIG(adapted_config, timeout, limits)

    struct container_config
    {
        cfg_vector<endpoint> endpoints;
        cfg_map<cfg_defaulted<cfg_int>> quantities;
        cfg_vector<cfg_defaulted<cfg_int>> priorities;
        cfg_optional<cfg_vector<cfg_int>> ports;
    };

    NOM_JCONFIG(container_config, endpoints, quantities, priorities, ports)

} // namespace mapping_fixture

TEST_CASE("defaulted scalars use native JSON conversion and retain null defaults", "[mapping]")
{
    nom::cfg_defaulted<nom::cfg_int> value{42};
    nlohmann::json(10).get_to(value);
    REQUIRE(*value == 10);
    const nlohmann::json serialized = value;
    REQUIRE(serialized == 10);

    nlohmann::json(nullptr).get_to(value);
    REQUIRE(*value == 10);
    REQUIRE_THROWS_AS(nlohmann::json(1.5).get_to(value), nlohmann::json::type_error);
}

TEST_CASE("defaulted exposes nested members through arrow and reference access", "[mapping]")
{
    nom::cfg_types::cfg_defaulted<mapping_fixture::endpoint> connection;
    connection->address = "primary";
    connection->timeout_s->value = 30;
    mapping_fixture::endpoint& endpoint = connection;
    endpoint.interface_name = "eth0";

    const nom::cfg_types::cfg_defaulted<mapping_fixture::endpoint>& constant = connection;
    const mapping_fixture::endpoint& constant_endpoint = constant;
    REQUIRE(&endpoint == &connection.value);
    REQUIRE(&constant_endpoint == &connection.value);
    REQUIRE(constant->address == "primary");
    REQUIRE(*constant->timeout_s == 30);
    REQUIRE((*constant).interface_name == "eth0");

    const nlohmann::json serialized = connection;
    REQUIRE(serialized == nlohmann::json{{"address", "primary"}, {"timeout_s", 30}, {"interface_name", "eth0"}});
}

TEST_CASE("empty config declarations still require and produce objects", "[mapping]")
{
    const auto config = nlohmann::json::object().get<mapping_fixture::empty_config>();
    const nlohmann::json serialized = config;
    REQUIRE(serialized == nlohmann::json::object());
    REQUIRE_THROWS_AS(nlohmann::json::array().get<mapping_fixture::empty_config>(), nlohmann::json::type_error);
}

TEST_CASE("required initializers do not make keys optional", "[mapping]")
{
    REQUIRE_THROWS_AS(nlohmann::json::object().get<mapping_fixture::app_config>(), nlohmann::json::out_of_range);
}

TEST_CASE("missing and null wrappers preserve defaults or represent absence", "[mapping]")
{
    using mapping_fixture::app_config;
    using nlohmann::json;

    const auto omitted = json{{"workers", 2}}.get<app_config>();
    REQUIRE(omitted.connection.get().timeout_s.get().get() == 42);
    REQUIRE(omitted.standby.has_value());
    REQUIRE(omitted.standby->timeout_s.get().get() == 43);
    REQUIRE_FALSE(omitted.note.has_value());

    const auto cleared =
        json{{"workers", 2}, {"connection", nullptr}, {"standby", nullptr}, {"note", nullptr}}.get<app_config>();
    REQUIRE(cleared.connection.get().address == "parent");
    REQUIRE_FALSE(cleared.standby.has_value());
    REQUIRE_FALSE(cleared.note.has_value());

    const json malformed{{"workers", 2}, {"connection", "bad"}};
    REQUIRE_THROWS_AS(malformed.get<app_config>(), json::type_error);
}

TEST_CASE("partial nested objects preserve parent defaults and engaged optionals", "[mapping]")
{
    const nlohmann::json document{
        {"workers", 2}, {"connection", {{"address", "primary"}}}, {"standby", {{"address", "secondary"}}}};
    const auto config = document.get<mapping_fixture::app_config>();
    REQUIRE(config.connection.get().address == "primary");
    REQUIRE(config.connection.get().timeout_s.get().get() == 42);
    REQUIRE(config.connection.get().interface_name == "eth0");
    REQUIRE(config.standby->address == "secondary");
    REQUIRE(config.standby->timeout_s.get().get() == 43);
    REQUIRE(config.standby->interface_name == "eth1");
}

TEST_CASE("get_to uses initialized values and constructs empty optionals", "[mapping]")
{
    mapping_fixture::app_config config;
    config.connection.get().timeout_s.get().value = 99;
    config.standby.reset();
    const nlohmann::json document{
        {"workers", 2}, {"connection", {{"address", "primary"}}}, {"standby", {{"address", "secondary"}}}};
    document.get_to(config);
    REQUIRE(config.workers.get() == 2);
    REQUIRE(config.connection.get().address == "primary");
    REQUIRE(config.connection.get().timeout_s.get().get() == 99);
    REQUIRE(config.standby->address == "secondary");
    REQUIRE(config.standby->timeout_s.get().get() == 5);
    REQUIRE_FALSE(config.standby->interface_name.has_value());
}

TEST_CASE("nested required fields still require explicit JSON keys", "[mapping]")
{
    const nlohmann::json document{{"workers", 2}, {"connection", {{"timeout_s", 7}}}};
    REQUIRE_THROWS_AS(document.get<mapping_fixture::app_config>(), nlohmann::json::out_of_range);
}

TEST_CASE("structures reject every non-object JSON shape", "[mapping]")
{
    for (const auto& document : {nlohmann::json(nullptr),
                                 nlohmann::json(1),
                                 nlohmann::json("text"),
                                 nlohmann::json::array(),
                                 nlohmann::json(true)})
    {
        REQUIRE_THROWS_AS(document.get<mapping_fixture::endpoint>(), nlohmann::json::type_error);
    }
}

TEST_CASE("config numeric members reject narrowing before conversion", "[mapping]")
{
    const nlohmann::json invalid_required{{"workers", 1.5}};
    REQUIRE_THROWS_AS(invalid_required.get<mapping_fixture::app_config>(), nlohmann::json::type_error);
    const nlohmann::json invalid_default{{"workers", 1}, {"connection", {{"address", "primary"}, {"timeout_s", 2.75}}}};
    REQUIRE_THROWS_AS(invalid_default.get<mapping_fixture::app_config>(), nlohmann::json::type_error);
}

TEST_CASE("member conversion failures retain native exception types", "[mapping]")
{
    const nlohmann::json invalid_address{{"address", 42}};
    REQUIRE_THROWS_AS(invalid_address.get<mapping_fixture::endpoint>(), nlohmann::json::type_error);
    const nlohmann::json invalid_optional{{"address", "local"}, {"interface_name", 42}};
    REQUIRE_THROWS_AS(invalid_optional.get<mapping_fixture::endpoint>(), nlohmann::json::type_error);
    const nlohmann::json invalid_required{{"workers", nullptr}};
    REQUIRE_THROWS_AS(invalid_required.get<mapping_fixture::app_config>(), nlohmann::json::type_error);
}

TEST_CASE("mapping errors identify nested fields and rejected numeric values", "[mapping]")
{
    using Catch::Matchers::ContainsSubstring;
    using nlohmann::json;

    SECTION("nested defaulted member")
    {
        const json document{{"workers", 1}, {"connection", {{"address", "primary"}, {"timeout_s", 2.0}}}};
        REQUIRE_THROWS_WITH(document.get<mapping_fixture::app_config>(),
                            ContainsSubstring("(/connection/timeout_s)") && ContainsSubstring("got 2.0"));
    }

    SECTION("nested optional member")
    {
        const json document{{"workers", 1}, {"standby", {{"address", "backup"}, {"timeout_s", 1.5}}}};
        REQUIRE_THROWS_WITH(document.get<mapping_fixture::app_config>(),
                            ContainsSubstring("(/standby/timeout_s)") && ContainsSubstring("got 1.5"));
    }

    SECTION("native string conversion")
    {
        const json document{{"workers", 1}, {"connection", {{"address", 42}}}};
        REQUIRE_THROWS_WITH(document.get<mapping_fixture::app_config>(), ContainsSubstring("(/connection/address)"));
    }

    SECTION("array element")
    {
        const json document{{"endpoints", json::array({{{"address", "primary"}, {"timeout_s", 2.0}}})},
                            {"quantities", json::object()},
                            {"priorities", json::array()}};
        REQUIRE_THROWS_WITH(document.get<mapping_fixture::container_config>(),
                            ContainsSubstring("(/endpoints/0/timeout_s)") && ContainsSubstring("got 2.0"));
    }

    SECTION("map key uses JSON pointer escaping")
    {
        const json document{
            {"endpoints", json::array()}, {"quantities", {{"a/b~c", "bad"}}}, {"priorities", json::array()}};
        REQUIRE_THROWS_WITH(document.get<mapping_fixture::container_config>(),
                            ContainsSubstring("(/quantities/a~1b~0c)") && ContainsSubstring(R"(got "bad")"));
    }
}

TEST_CASE("config supports checked numeric types and native boolean members", "[mapping]")
{
    const nlohmann::json document{{"count", 3}, {"rate", 1.5}, {"active", true}, {"limit", -1}};
    const auto config = document.get<mapping_fixture::numeric_config>();
    REQUIRE(config.count.get() == 3);
    REQUIRE(config.rate.get() == 1.5);
    REQUIRE(config.active);
    REQUIRE(config.limit->get() == -1);
    const nlohmann::json serialized = config;
    REQUIRE(serialized == document);

    const nlohmann::json malformed{{"count", 3}, {"rate", 1.5}, {"active", true}, {"limit", 1.5}};
    REQUIRE_THROWS_AS(malformed.get<mapping_fixture::numeric_config>(), nlohmann::json::type_error);
}

TEST_CASE("native containers compose with defaults and optional members", "[mapping]")
{
    const nlohmann::json document{{"endpoints", nlohmann::json::array({{{"address", "primary"}}})},
                                  {"quantities", {{"new", 6}}},
                                  {"priorities", nlohmann::json::array({2, 3})},
                                  {"ports", nlohmann::json::array({80, 443})}};
    mapping_fixture::container_config config{{{"old", {99}, std::nullopt}}, {{"old", {99}}}, {{99}}, std::nullopt};
    document.get_to(config);
    REQUIRE(config.endpoints.size() == 1);
    REQUIRE(config.endpoints.front().address == "primary");
    REQUIRE(config.endpoints.front().timeout_s.get().get() == 5);
    REQUIRE(config.quantities.size() == 1);
    REQUIRE(config.quantities.at("new").get().get() == 6);
    REQUIRE(config.priorities.size() == 2);
    REQUIRE(config.priorities[0].get().get() == 2);
    REQUIRE(config.priorities[1].get().get() == 3);
    REQUIRE(config.ports.has_value());
    REQUIRE(config.ports->size() == 2);
    REQUIRE(config.ports->at(0).get() == 80);
    REQUIRE(config.ports->at(1).get() == 443);

    const nlohmann::json serialized = config;
    REQUIRE(serialized.at("quantities") == document.at("quantities"));
    REQUIRE(serialized.at("priorities") == document.at("priorities"));
    REQUIRE(serialized.at("ports") == document.at("ports"));
    const auto restored = serialized.get<mapping_fixture::container_config>();
    REQUIRE(restored.endpoints.front().address == "primary");
    REQUIRE(restored.quantities.at("new").get().get() == 6);
    REQUIRE(restored.ports->at(0).get() == 80);
    REQUIRE(restored.ports->at(1).get() == 443);
}

TEST_CASE("serialization emits wrapped values and explicit absent optionals", "[mapping]")
{
    const auto original = nlohmann::json{{"workers", 3}}.get<mapping_fixture::app_config>();
    const nlohmann::json encoded = original;
    REQUIRE(encoded.at("connection").at("timeout_s") == 42);
    REQUIRE(encoded.at("note").is_null());
    REQUIRE(encoded.at("standby").at("interface_name") == "eth1");
    const auto restored = encoded.get<mapping_fixture::app_config>();
    REQUIRE(restored.workers.get() == original.workers.get());
    REQUIRE(restored.connection.get().address == original.connection.get().address);
    REQUIRE(restored.connection.get().timeout_s.get().get() == 42);
    REQUIRE(restored.standby->timeout_s.get().get() == 43);
    REQUIRE_FALSE(restored.note.has_value());
}

TEST_CASE("native ADL supports generic and concrete converters", "[mapping]")
{
    const nlohmann::json document{{"timeout", 250}, {"limits", {{"connection-limit", 8}}}};
    const auto config = document.get<mapping_fixture::adapted_config>();
    REQUIRE(config.timeout.value == std::chrono::milliseconds{250});
    REQUIRE(config.limits.connection_limit.get() == 8);
    const nlohmann::json encoded = config;
    REQUIRE(encoded == document);
}

TEST_CASE("signed config integers enforce JSON type and representable range", "[mapping]")
{
    using nlohmann::json;
    using nom::cfg_types::cfg_int;

    for (const std::int64_t number :
         {std::numeric_limits<std::int64_t>::min(), std::int64_t{0}, std::numeric_limits<std::int64_t>::max()})
    {
        const json document = number;
        const auto value = document.get<cfg_int>();
        REQUIRE(value.get() == number);
        const json encoded = value;
        REQUIRE(encoded == document);
    }
    for (const auto& document : {json(1.5), json(1.0), json("1"), json(true), json(nullptr)})
    {
        REQUIRE_THROWS_AS(document.get<cfg_int>(), json::type_error);
    }
    const json maximum_unsigned = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    REQUIRE(maximum_unsigned.get<cfg_int>().get() == std::numeric_limits<std::int64_t>::max());
    const json overflow = std::numeric_limits<std::uint64_t>::max();
    REQUIRE_THROWS_AS(overflow.get<cfg_int>(), json::out_of_range);
}

TEST_CASE("unsigned config integers reject negative and noninteger values", "[mapping]")
{
    using nlohmann::json;
    using nom::cfg_types::cfg_uint;

    for (const auto& document : {json(std::int64_t{0}),
                                 json(std::uint64_t{0}),
                                 json(std::uint64_t{42}),
                                 json(std::numeric_limits<std::uint64_t>::max())})
    {
        const auto value = document.get<cfg_uint>();
        const json encoded = value;
        REQUIRE(encoded == document);
    }
    REQUIRE_THROWS_AS(json(-1).get<cfg_uint>(), json::out_of_range);
    for (const auto& document : {json(1.5), json(1.0), json("1"), json(true), json(nullptr)})
    {
        REQUIRE_THROWS_AS(document.get<cfg_uint>(), json::type_error);
    }
}

TEST_CASE("floating config numbers support finite numbers and reject nonfinite values", "[mapping]")
{
    using nlohmann::json;
    using nom::cfg_types::cfg_double;

    for (const auto& document : {json(-1),
                                 json(std::uint64_t{1}),
                                 json(1.5),
                                 json(std::numeric_limits<double>::max()),
                                 json(std::numeric_limits<double>::lowest())})
    {
        const auto value = document.get<cfg_double>();
        REQUIRE(value.get() == document.get<double>());
        const json encoded = value;
        REQUIRE(encoded == document);
    }
    for (const auto& document : {json("1.5"), json(true), json(nullptr)})
    {
        REQUIRE_THROWS_AS(document.get<cfg_double>(), json::type_error);
    }
    for (const double number : {std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN()})
    {
        REQUIRE_THROWS_AS(json(number).get<cfg_double>(), json::out_of_range);
        const cfg_double value{number};
        REQUIRE_THROWS_AS(json(value), json::out_of_range);
    }
}

TEST_CASE("mapping and application validation are distinct stages", "[mapping]")
{
    const auto mapped = nlohmann::json{{"port", -1}}.get<mapping_fixture::port_config>();
    REQUIRE(mapped.port.get() == -1);
    const auto invalid = mapped.validate();
    REQUIRE_FALSE(invalid.has_value());
    REQUIRE(invalid.error() == "port must be between 1 and 65535");
    const auto valid = nlohmann::json{{"port", 443}}.get<mapping_fixture::port_config>();
    REQUIRE(valid.validate().has_value());
}
