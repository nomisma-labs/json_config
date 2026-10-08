# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

# Integration checks for library usage and installation (docs/kit/howto.md#library-usage-integration-tests).

# template-only:begin
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

option(CMAKE_TEMPLATE_TEST_PYTHON_PACKAGE "Build and test installed native Python wheels" OFF)
if(CMAKE_TEMPLATE_TEST_PYTHON_PACKAGE)
    if(NOT NOM_PYTHON_EXECUTABLE)
        message(FATAL_ERROR "Python wheel tests require NOM_PYTHON_EXECUTABLE. "
            "Create build/python-env with python3 -m venv, then install "
            "tools/kit/python_package_requirements.txt with that environment's pip.")
    endif()
    execute_process(COMMAND ${NOM_PYTHON_EXECUTABLE} -c "import build, pip, scikit_build_core"
        RESULT_VARIABLE python_package_dependencies OUTPUT_QUIET ERROR_QUIET)
    if(NOT python_package_dependencies EQUAL 0)
        message(FATAL_ERROR "NOM_PYTHON_EXECUTABLE must provide build, pip and scikit-build-core. "
            "Install tools/kit/python_package_requirements.txt with that environment's pip.")
    endif()
    find_program(python_package_ninja ninja REQUIRED)
    add_test(NAME ${PROJECT_NAME}_python_package
        COMMAND ${NOM_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/python_package/check.py
            --source ${PROJECT_SOURCE_DIR} --python ${NOM_PYTHON_EXECUTABLE}
            --cmake ${CMAKE_COMMAND} --compiler ${CMAKE_CXX_COMPILER}
            --ninja ${python_package_ninja})
    set_tests_properties(${PROJECT_NAME}_python_package PROPERTIES
        LABELS "integration;python_package" TIMEOUT 300)
endif()

add_test(NAME ${PROJECT_NAME}_app_presets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/app_presets/check.py
        --cmake ${CMAKE_COMMAND}
        --compiler ${CMAKE_CXX_COMPILER}
        --generator "${CMAKE_GENERATOR}"
        --kit ${PROJECT_SOURCE_DIR}/cmake/kit)
set_tests_properties(${PROJECT_NAME}_app_presets PROPERTIES LABELS integration)

add_test(NAME ${PROJECT_NAME}_template_sync
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/template_sync/check.py
        --script ${PROJECT_SOURCE_DIR}/tools/kit/sync_template.py)
set_tests_properties(${PROJECT_NAME}_template_sync PROPERTIES LABELS integration)

if(UNIX)
    add_test(NAME ${PROJECT_NAME}_relocation
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/relocation/check.py
            --cmake ${CMAKE_COMMAND} --compiler ${CMAKE_CXX_COMPILER}
            --generator "${CMAKE_GENERATOR}" --kit ${PROJECT_SOURCE_DIR}/cmake/kit)
    set_tests_properties(${PROJECT_NAME}_relocation PROPERTIES LABELS integration)
endif()

if(nom_install)
    nom_library_usage_tests(library_usage)
endif()
# The app build must collect its source config into the common build/config tree.
if(TARGET ${PROJECT_NAME}_hello)
    add_test(NAME ${PROJECT_NAME}_collected_config
        COMMAND ${CMAKE_COMMAND} -E compare_files
            ${PROJECT_SOURCE_DIR}/apps/hello/config/settings.json
            ${CMAKE_BINARY_DIR}/config/hello/settings.json)
    set_tests_properties(${PROJECT_NAME}_collected_config PROPERTIES LABELS integration)
    if(nom_install)
        add_test(NAME ${PROJECT_NAME}_installed_config
            COMMAND ${CMAKE_COMMAND} -E compare_files
                ${CMAKE_BINARY_DIR}/config/hello/settings.json
                ${PROJECT_BINARY_DIR}/tests/integration/library_usage/install/config/hello/settings.json)
        set_tests_properties(${PROJECT_NAME}_installed_config PROPERTIES
            FIXTURES_REQUIRED ${PROJECT_NAME}_installed LABELS integration)
    endif()
endif()
# template-only:end
