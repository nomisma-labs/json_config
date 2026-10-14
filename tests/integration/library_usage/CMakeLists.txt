# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

cmake_minimum_required(VERSION 3.30)
project(json_config_library_usage LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/kit/nom_kit.cmake")

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

# The model compiles with common types alone; its JSON binding belongs to the consumer.
nom_lib(config_model STATIC MODULES config_model.cppm
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    PUBLIC_DEPS nom::config_types)

add_executable(use_library use_library_test.cxx)
target_link_libraries(use_library PRIVATE nom::config_model nom::json_config)
add_test(NAME use_library COMMAND use_library)

# Generation is test tooling; the package consumer still links only installed targets.
find_package(Python 3.10 REQUIRED COMPONENTS Interpreter)
set(generated_config_dir "${CMAKE_CURRENT_BINARY_DIR}/genconfig")
add_test(NAME generate_consumer_config
    COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/jcfggen.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}/consumer_jcfg.json"
        --output-dir "${generated_config_dir}")
set_tests_properties(generate_consumer_config PROPERTIES FIXTURES_SETUP consumer_generated_config)
add_test(NAME load_generated_config
    COMMAND use_library --config "${generated_config_dir}/prod/consumer.json" --check-config)
add_test(NAME reject_generated_config
    COMMAND use_library --config "${generated_config_dir}/invalid/consumer.json"
        --check-config --expect-validation-error)
set_tests_properties(load_generated_config reject_generated_config PROPERTIES
    FIXTURES_REQUIRED consumer_generated_config)

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
