// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <json_config/config.h>

#include <expected>
#include <optional>
#include <string>

import nom.json_config;

namespace consumer
{
    using namespace nom::jcfg::types;

    struct connection_config
    {
        std::string address;
        defaulted<jint> timeout_s{5};
        std::optional<std::string> interface_name;
    };

    NOM_CONFIG(connection_config, address, timeout_s, interface_name)

    struct app_config
    {
        defaulted<connection_config> connection{{"", {42}, "loopback"}};
        defaulted<bool> enabled{true};
        std::optional<std::string> annotation;

        std::expected<void, std::string> validate() const
        {
            if (connection->address.empty() || *connection->timeout_s <= 0)
            {
                return std::unexpected("connection requires an address and a positive timeout");
            }
            return {};
        }
    };

    NOM_CONFIG(app_config, connection, enabled, annotation)
} // namespace consumer

int main()
{
    const nlohmann::json document{{"connection", {{"address", "wss://example.test/feed"}}}};
    const auto config = document.get<consumer::app_config>();
    const consumer::connection_config& connection = config.connection;
    if (!config.validate() || *connection.timeout_s != 42 || connection.interface_name != "loopback" || !config.enabled)
    {
        return 1;
    }

    const nlohmann::json serialized = config;
    if (serialized.at("connection").at("timeout_s") != 42 || !serialized.at("annotation").is_null())
    {
        return 2;
    }
    const auto restored = serialized.get<consumer::app_config>();
    if (!restored.validate() || restored.connection->address != config.connection->address)
    {
        return 3;
    }

    auto existing = config;
    consumer::connection_config& existing_connection = existing.connection;
    existing_connection.timeout_s->value = 90;
    document.get_to(existing);
    if (*existing.connection->timeout_s != 90)
    {
        return 4;
    }

    auto malformed = document;
    malformed["connection"]["timeout_s"] = "bad";
    try
    {
        malformed.get_to(existing);
        return 5;
    }
    catch (const nlohmann::json::type_error& error)
    {
        const std::string message = error.what();
        if (!message.contains("(/connection/timeout_s)") || !message.contains(R"(got "bad")"))
        {
            return 6;
        }
    }
    return 0;
}
