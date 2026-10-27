# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(locator_tests SOURCES locator_test.cxx DEPS nom::json_config)
if(TARGET ${PROJECT_NAME}_locator_tests)
    nom_enable_std_modules(${PROJECT_NAME}_locator_tests)
endif()
