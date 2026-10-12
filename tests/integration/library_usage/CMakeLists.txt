# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

cmake_minimum_required(VERSION 3.30)
project(json_config_library_usage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBRARY_USAGE_MODE "package" CACHE STRING "package: find_package(json_config); source: FetchContent")

if(LIBRARY_USAGE_MODE STREQUAL "package")
    find_package(json_config CONFIG REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(json_config SOURCE_DIR ${LIBRARY_USAGE_SOURCE_DIR})
    FetchContent_MakeAvailable(json_config)
endif()

enable_testing()

add_executable(use_library use_library_test.cxx)
target_link_libraries(use_library PRIVATE nom::json_config)
add_test(NAME use_library COMMAND use_library)

foreach(test_case IN ITEMS integer_member integer_container unregistered_type)
    add_executable(reject_${test_case} EXCLUDE_FROM_ALL reject_config_type_test.cxx)
    target_link_libraries(reject_${test_case} PRIVATE nom::json_config)
    string(TOUPPER "${test_case}" definition)
    target_compile_definitions(reject_${test_case} PRIVATE "NOM_TEST_${definition}")
endforeach()
add_test(NAME reject_disabled_config_types
    COMMAND ${CMAKE_COMMAND} "-Dbuild_dir=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake")
set_tests_properties(reject_disabled_config_types PROPERTIES RUN_SERIAL TRUE)
