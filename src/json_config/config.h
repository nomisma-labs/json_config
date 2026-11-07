// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

#pragma once

#include <json_config/nlohmann_json.h>
#include <type_traits>

// Evaluate the application's definition at its call site, outside compiled library code.
#define NOM_JCFG_STRINGIFY_IMPL(value) #value
#define NOM_JCFG_STRINGIFY(value) NOM_JCFG_STRINGIFY_IMPL(value)
#ifdef NOM_CONFIG_KEY
#define NOM_CONFIG_KEY_VALUE NOM_JCFG_STRINGIFY(NOM_CONFIG_KEY)
#else
#define NOM_CONFIG_KEY_VALUE ""
#endif

#define NOM_JCFG_READ_MEMBER(member)                                                                                   \
    static_assert(::nom::is_config_member<std::remove_cvref_t<decltype(nom_jcfg_destination.member)>>,                 \
                  "NOM_JCONFIG member type is not config-enabled");                                                    \
    ::nom::jcfg::detail::read_member(nom_jcfg_document, #member, nom_jcfg_destination.member);
#define NOM_JCFG_WRITE_MEMBER(member)                                                                                  \
    static_assert(::nom::is_config_member<std::remove_cvref_t<decltype(nom_jcfg_source.member)>>,                      \
                  "NOM_JCONFIG member type is not config-enabled");                                                    \
    nom_jcfg_document[#member] = nom_jcfg_source.member;

// Declare beside the type, in its namespace, after importing nom.json_config.
#define NOM_JCONFIG(Type, ...)                                                                                         \
    inline std::true_type nom_config_type(Type*)                                                                       \
    {                                                                                                                  \
        return {};                                                                                                     \
    }                                                                                                                  \
    inline void from_json(const ::nlohmann::json& nom_jcfg_document, [[maybe_unused]] Type& nom_jcfg_destination)      \
    {                                                                                                                  \
        (void)nom_jcfg_document.get_ref<const ::nlohmann::json::object_t&>();                                          \
        __VA_OPT__(NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NOM_JCFG_READ_MEMBER, __VA_ARGS__)))                       \
    }                                                                                                                  \
    inline void to_json(::nlohmann::json& nom_jcfg_document, [[maybe_unused]] const Type& nom_jcfg_source)             \
    {                                                                                                                  \
        nom_jcfg_document = ::nlohmann::json::object();                                                                \
        __VA_OPT__(NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NOM_JCFG_WRITE_MEMBER, __VA_ARGS__)))                      \
    }

// Declare in the enum's namespace with {{Type::value, "name"}, ...}.
// Matching is exact; the first name for a value is its serialized form.
#define NOM_JCONFIG_ENUM(Type, ...)                                                                                    \
    static_assert(std::is_enum_v<Type>, "NOM_JCONFIG_ENUM requires an enum type");                                     \
    inline std::true_type nom_config_type(Type*)                                                                       \
    {                                                                                                                  \
        return {};                                                                                                     \
    }                                                                                                                  \
    inline void from_json(const ::nlohmann::json& nom_jcfg_document, Type& nom_jcfg_destination)                       \
    {                                                                                                                  \
        static constexpr std::pair<Type, std::string_view> nom_jcfg_names[] = __VA_ARGS__;                             \
        const auto& nom_jcfg_name = nom_jcfg_document.get_ref<const ::nlohmann::json::string_t&>();                    \
        for (const auto& [nom_jcfg_value, nom_jcfg_text] : nom_jcfg_names)                                             \
        {                                                                                                              \
            if (nom_jcfg_name == nom_jcfg_text)                                                                        \
            {                                                                                                          \
                nom_jcfg_destination = nom_jcfg_value;                                                                 \
                return;                                                                                                \
            }                                                                                                          \
        }                                                                                                              \
        throw ::nlohmann::json::type_error::create(                                                                    \
            302, "unmapped " #Type " value " + nom_jcfg_document.dump(), &nom_jcfg_document);                          \
    }                                                                                                                  \
    inline void to_json(::nlohmann::json& nom_jcfg_document, Type nom_jcfg_source)                                     \
    {                                                                                                                  \
        static constexpr std::pair<Type, std::string_view> nom_jcfg_names[] = __VA_ARGS__;                             \
        for (const auto& [nom_jcfg_value, nom_jcfg_text] : nom_jcfg_names)                                             \
        {                                                                                                              \
            if (nom_jcfg_source == nom_jcfg_value)                                                                     \
            {                                                                                                          \
                nom_jcfg_document = nom_jcfg_text;                                                                     \
                return;                                                                                                \
            }                                                                                                          \
        }                                                                                                              \
        throw ::nlohmann::json::type_error::create(                                                                    \
            302, "unmapped " #Type " value " + std::to_string(std::to_underlying(nom_jcfg_source)), nullptr);          \
    }
