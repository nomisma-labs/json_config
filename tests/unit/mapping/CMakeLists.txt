# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_c2test(mapping_tests SOURCES mapping_test.cxx DEPS nom::json_config)
set_tests_properties(${PROJECT_NAME}_mapping_tests PROPERTIES LABELS unit)
