# SPDX-FileCopyrightText: 2026 Alexey Tkachenko
# SPDX-License-Identifier: LicenseRef-Nomisma-Labs-Proprietary

nom_app(config_args_example
    SOURCES main.cpp app_arguments.cpp
    DEPS nom::json_config Boost::program_options
    COMPILE_OPTIONS -DNOM_CONFIG_KEY=config_args_example)
if(TARGET config_args_example)
    nom_enable_std_modules(config_args_example)
endif()
