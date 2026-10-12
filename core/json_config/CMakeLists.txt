# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_lib(json_config
    MODULES json_config.cppm config_types.cppm
    HEADERS config.h
    PUBLIC_DEPS nlohmann_json::nlohmann_json)

# Keep diagnostic paths available in our converters and source/package consumers.
target_compile_definitions(json_config PUBLIC JSON_DIAGNOSTICS=1)

if(nom_install)
    nom_install_package(TARGETS json_config DEPENDENCIES nlohmann_json)
endif()
