# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(runtime_tests SOURCES runtime_test.cxx DEPS nom::json_config)
if(TARGET ${PROJECT_NAME}_runtime_tests)
    nom_enable_std_modules(${PROJECT_NAME}_runtime_tests)
endif()
target_compile_definitions(${PROJECT_NAME}_runtime_tests PRIVATE NOM_CONFIG_KEY=runtime_config)
