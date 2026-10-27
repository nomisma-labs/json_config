# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(loader_tests SOURCES loader_test.cxx DEPS nom::json_config)
if(TARGET ${PROJECT_NAME}_loader_tests)
    nom_enable_std_modules(${PROJECT_NAME}_loader_tests)
endif()
