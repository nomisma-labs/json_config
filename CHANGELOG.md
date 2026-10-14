# Changelog

Notable changes to this repo. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org); how to release:
[docs/kit/build.md](docs/kit/build.md#versioning).

## [Unreleased]

- Separate format-independent types into this repository's `nom.config_types`
  module. Its target is a transitive dependency of source and installed JSON
  config consumers and is installed with the `json_config` package.
- Replace `nom::jcfg::jint`, `juint`, `jdouble`, and `defaulted<T>` with
  `nom::cfg_int`, `cfg_uint`, `cfg_double`, and `cfg_defaulted<T>`. Use
  `nom::cfg_types` for convenience aliases, including strings, booleans,
  containers, and optionals; custom type opt-in is now `nom::enable_config_type`.
- Rename the JSON mapping macro from `NOM_CONFIG` to `NOM_JCONFIG`. JSON numeric
  conversion checks and existing `null` behavior are retained. The `NOM_CONFIG`
  environment variable and `NOM_CONFIG_KEY` application setting keep their names.
- Sync the shared build infrastructure to `cmake_template` v0.9.0. Public build,
  test, Python, and installation helpers enforce their project-local options;
  library usage tests retain source coverage with installation disabled.
- Make `nom_jcfg_generate()` respect `nom_build_apps` and `nom_jcfg_test()` require
  both apps and tests. Simplify helper calls while retaining guards for direct
  test registration and target/test property operations.
- Initialize the application configuration project from `cmake_template`, retaining
  its pinned nlohmann/json dependency and shared build infrastructure.
- Sync the shared build infrastructure to `cmake_template` v0.8.2, including
  `COMPILE_OPTIONS` support and git hooks; remove the unused Metal dependency.
- Sync the shared build infrastructure to `cmake_template` v0.8.3 and find Boost's
  program_options, filesystem and iostreams libraries with `nom_find_boost()`.
