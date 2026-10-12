// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#include <json_config/config.h>

#include <vector>

import nom.json_config;

namespace rejected_fixture
{
    struct unregistered_type
    {
        int value{};
    };

    inline void from_json(const nlohmann::json& document, unregistered_type& destination)
    {
        document.get_to(destination.value);
    }

    inline void to_json(nlohmann::json& document, const unregistered_type& source)
    {
        document = source.value;
    }

    struct config
    {
#if defined(NOM_TEST_INTEGER_MEMBER)
        int value{};
#elif defined(NOM_TEST_INTEGER_CONTAINER)
        std::vector<int> value;
#elif defined(NOM_TEST_UNREGISTERED_TYPE)
        unregistered_type value;
#else
#error Select a disabled config type test
#endif
    };

    NOM_CONFIG(config, value)
} // namespace rejected_fixture

int main()
{
    const nlohmann::json document{{"value", 1}};
    const auto config = document.get<rejected_fixture::config>();
    const nlohmann::json serialized = config;
    return serialized.empty();
}
