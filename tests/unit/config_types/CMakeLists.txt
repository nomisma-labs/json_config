# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(config_types_tests SOURCES config_types_test.cxx DEPS nom::config_types)
if(nom_build_tests)
    set_tests_properties(${PROJECT_NAME}_config_types_tests PROPERTIES LABELS unit)
endif()
