# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

# Integration checks for library usage and installation (docs/kit/howto.md#library-usage-integration-tests).
add_subdirectory(jcfggen)
add_subdirectory(config_build)
nom_library_usage_tests(library_usage)
