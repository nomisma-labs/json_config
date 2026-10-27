# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(mapping_tests SOURCES mapping_test.cxx DEPS nom::json_config)
if(TARGET ${PROJECT_NAME}_mapping_tests)
    nom_enable_std_modules(${PROJECT_NAME}_mapping_tests)
endif()
