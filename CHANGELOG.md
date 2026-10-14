# Changelog

Notable changes to this repo. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org); how to release:
[docs/kit/build.md](docs/kit/build.md#versioning).

## [Unreleased]

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
