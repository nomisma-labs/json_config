# Changelog

Notable changes to this repo. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org); how to release:
[docs/kit/build.md](docs/kit/build.md#versioning).

## [Unreleased]

## [0.3.0] - 2026-10-28

### Changed

- Sync cmake_template v0.12.0, use `IMPORT_STD` in target helpers, and suppress
  experimental-feature warnings through the shared presets while retaining other warnings.
- Pin base v0.3.0 for standalone and composed builds.

## [0.2.1] - 2026-10-28

### Changed

- Update the `base` submodule to v0.2.1 so standalone and composed builds use the same
  standard-library module support and common configuration types.
- Sync the shared build kit to cmake_template v0.11.2 and adopt `import std;` in C++
  modules and consumers, retaining macro and third-party header boundaries.
  Include the follow-up standard-library documentation correction from template commit `8ad7a4e`.
- Require CMake 4.4.x and enable standard-library module detection before `project()`
  in both the repository and source/installed-package consumer projects.

## [0.2.0] - 2026-10-27

- Sync the shared build infrastructure with cmake_template v0.11.1; `nom_app()` uses the
  supplied target name without a project prefix.
  Update config-generation helpers, app references and build tests to use those targets;
  generated-config CTest names keep their project prefix.

- Move ownership of `nom.config_types` and `nom::config_types` to the required `base`
  v0.2.0 dependency. `json_config` no longer defines or installs a second common-types module.
  JSON consumers retain the same imports and targets; standalone common-type consumers
  use `find_package(base)`. Common-type tests and documentation now live in `base`.

## [0.1.0] - 2026-10-15

The first complete application configuration workflow: define layered configs in
**jcfg**, generate ordinary JSON, and load validated C++ configuration structures.

The [jcfg language guide](docs/json_config/jcfg.md) establishes the specification
through worked examples: single inheritance, recursive merging, whole-file and
fragment imports, deletion, replacement, and generation-time variables. It also
defines runtime variables and overrides. Companion guides document the four
components that turn this model into an application workflow:

- **[Python configuration generation](docs/json_config/config_generation.md).**
  A reusable engine resolves selected sections in memory; `jcfggen.py` discovers
  sources, lists section metadata, and publishes generated configs while preserving
  unchanged output files. Both use Python 3.10+ and the standard library.
- **[Build-system integration](docs/json_config/build_system_integration.md).**
  Source configs are collected using the shared build kit. The library's CMake
  helpers add opt-in generation, per-section application tests, and installation
  of generated configs. Repository-wide `config` and `clean_config` targets prepare
  or remove collected and generated configs; normal build cleanup covers them too.
- **[C++ config location and loading](docs/json_config/app_config_binding.md).**
  Applications select configs through command-line options, environment settings,
  or executable-based lookup. Loading applies runtime variables and overrides,
  maps to C++, and runs application validation. Configurations composed entirely
  of defaulted or optional fields can initialize without a config file. The API reports
  the selected source and contextual errors, supports `--check-config`, and leaves
  application-specific arguments for the application's own parser.
- **[JSON/C++ mapping](docs/json_config/json_cpp_mapping.md).**
  `NOM_JCONFIG` uses nlohmann/json to map required, defaulted, optional, and nested
  fields. Format-independent `nom::cfg_*` types live in `nom.config_types`, with
  checked JSON numeric conversions supplied by the mapping layer. Application
  structures own their validation and explicitly delegate to their children. The public
  `nom.json_config` module and its common types are available to source consumers
  and through an installed CMake package.

Runnable examples connect the specification to application code:
[md_capture](apps/md_capture/README.md) implements the guide's comprehensive
configuration, [config_workflow_example](apps/config_workflow_example/README.md)
covers the build, generation, loading, testing, and installation workflow, and
[config_args_example](apps/config_args_example/README.md) combines config options
with application arguments.

Unit and integration tests cover the language, generator CLI, build lifecycle,
mapping, locator, and loader. CTest exercises generation through application
validation, including failure diagnostics and startup suppression, with separate
consumer projects testing both source integration and installed-package usage.
