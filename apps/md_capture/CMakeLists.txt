# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_app(md_capture SOURCES main.cpp DEPS nom::json_config
    COMPILE_OPTIONS -DNOM_CONFIG_KEY=md_capture)
if(TARGET md_capture)
    nom_enable_std_modules(md_capture)
endif()

nom_jcfg_generate(md_capture SOURCE md_capture_jcfg.json BUILD_WITH_APP)
nom_jcfg_test(md_capture base)
nom_jcfg_test(md_capture ci)
nom_jcfg_test(md_capture prod)
nom_jcfg_test(md_capture prod-liquidation)
