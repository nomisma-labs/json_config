# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_app(config_example SOURCES main.cpp DEPS nom::json_config
    COMPILE_OPTIONS -DNOM_CONFIG_KEY=config_example)

nom_jcfg_generate(config_example SOURCE config_example_jcfg.json BUILD_WITH_APP)

if(nom_build_tests)
    nom_jcfg_test(config_example dev)
    nom_jcfg_test(config_example prod)

    add_test(NAME ${PROJECT_NAME}_config_example_check
        COMMAND $<TARGET_FILE:${PROJECT_NAME}_config_example>
            --config "${NOM_COLLECTION_CONFIG_DIR}/config_example/config_example.json"
            --set /timeout_s=20 --check-config)
    set_tests_properties(${PROJECT_NAME}_config_example_check PROPERTIES LABELS integration)
endif()
