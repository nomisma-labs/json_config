# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(locator_tests SOURCES locator_test.cxx DEPS nom::json_config)
set_tests_properties(${PROJECT_NAME}_locator_tests PROPERTIES LABELS unit)
