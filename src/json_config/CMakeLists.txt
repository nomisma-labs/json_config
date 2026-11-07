# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(json_config_os_source util/os_linux.cpp)
else()
    message(FATAL_ERROR "json_config executable discovery is not implemented for ${CMAKE_SYSTEM_NAME}")
endif()

nom_lib(json_config IMPORT_STD
    MODULES json_config.cppm config_mapping.cppm config_runtime.cppm
    SOURCES config_options.cpp config_locator.cpp config_loader.cpp ${json_config_os_source}
    HEADERS config.h nlohmann_json.h
    PUBLIC_DEPS nom::config_types nlohmann_json::nlohmann_json
    PRIVATE_DEPS Boost::program_options)

# Keep diagnostic paths available in our converters and source/package consumers.
target_compile_definitions(json_config PUBLIC JSON_DIAGNOSTICS=1)

nom_install_package(TARGETS json_config DEPENDENCIES base nlohmann_json Boost)
