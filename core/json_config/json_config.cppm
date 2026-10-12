// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

module;

#include <optional>

export module nom.json_config;

export import :types;

export namespace nom::jcfg
{
    namespace detail
    {
        // NOM_CONFIG supplies nlohmann::json; these helpers only handle presence.
        template <typename Json, typename Value>
        void read_member(const Json& document, const char* name, Value& destination)
        {
            document.at(name).get_to(destination);
        }

        template <typename Json, typename Value>
        void read_member(const Json& document, const char* name, defaulted<Value>& destination)
        {
            if (const auto found = document.find(name); found != document.end())
            {
                found->get_to(destination);
            }
        }

        template <typename Json, typename Value>
        void read_member(const Json& document, const char* name, std::optional<Value>& destination)
        {
            const auto found = document.find(name);
            if (found == document.end())
            {
                return;
            }
            if (found->is_null())
            {
                destination.reset();
            }
            else
            {
                if (!destination)
                {
                    destination.emplace();
                }
                found->get_to(*destination);
            }
        }
    } // namespace detail
} // namespace nom::jcfg
