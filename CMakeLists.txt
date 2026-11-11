# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 4.4)

include(cmake/kit/std_modules.cmake)

# The version lives only here; releases are tagged v<version> (docs/kit/build.md#versioning).
project(json_config
    VERSION 0.7.0
    DESCRIPTION "JSON-based application configuration library for Nomisma"
    LANGUAGES CXX)

include(cmake/kit/nom_kit.cmake)
nom_project_options()

if(nom_install)
    include(GNUInstallDirs)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME})
endif()
include(cmake/json_config/config.cmake)

# Dependencies come from external/ submodules (docs/kit/howto.md#a-dependency). In a superbuild the
# outermost FetchContent_Declare wins, so every repo shares one copy.

nom_require_submodule(external/base)
FetchContent_Declare(base SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/base)
FetchContent_MakeAvailable(base)

# nlohmann_json's own CMake project works as a subproject, so it is used as is.
nom_require_submodule(external/nlohmann_json)
set(JSON_Install ${nom_install}) # find_package consumers need nlohmann next to our package
FetchContent_Declare(nlohmann_json SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/nlohmann_json SYSTEM)
FetchContent_MakeAvailable(nlohmann_json)

# Boost comes from system packages (docs/kit/build.md#dependencies).
nom_find_boost()

# Discover Catch2 before any code directory can register an inline test suite.
if(nom_build_tests)
    enable_testing()
    nom_find_catch2()
endif()

add_subdirectory(src)

if(nom_build_apps)
    add_subdirectory(apps)
endif()

if(nom_build_tests)
    add_subdirectory(tests)
endif()
