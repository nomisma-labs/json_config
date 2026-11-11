# Changelog

Notable changes to this repo. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org); how to release:
[docs/kit/build.md](docs/kit/build.md#versioning).

## [Unreleased]

## [0.7.1] - 2026-11-12

### Changed

- Pin `base v0.6.0` from its new repository history. Public APIs and runtime behavior
  are unchanged; recursive checkouts use the current base release.

## [0.7.0] - 2026-11-11

### Changed

- License project-owned code and documentation under MIT, retaining
  Alexey Tkachenko's copyright. Third-party dependencies retain their own licenses.
- Synchronize cmake_template v0.14.0 and record the repository license policy;
  the shared kit retains MIT independently of the project selection.
- Include the project license in installed artifacts alongside the applicable
  toolchain and dependency notices.
- Update internal dependency pins to `base v0.5.0`.

## [0.6.0] - 2026-11-08

### Changed

- Move library sources from `core/` to `src/` and synchronize shared build tooling with
  cmake_template v0.13.0. Module names, CMake targets, public includes and installed paths
  remain unchanged.
- Pin base v0.4.0 so the library and its source dependency use the same `src/` layout.

### Upgrade

- Source consumers must adopt cmake_template v0.13.0 and migrate their library sources to
  `src/` together with their dependency pins. Recreate build trees and compilation databases.

## [0.5.0] - 2026-11-07

### Added

- `NOM_JCONFIG_ENUM` registers config enum types and maps explicit string names,
  rejecting unmapped JSON and enum values. It composes with required, defaulted,
  optional and container config members.

## [0.4.0] - 2026-11-06

### Added

- Loadable Bash helpers `cfg_set`, `cfg_add`, `cfg_print`, and `cfg_clear` for
  managing `NOM_CONFIG_DIR` in the current shell. Setting or adding a directory
  prints the resulting search path.
- Generated configuration help through `print_config_help()`, `--help`/`-h`, and
  short aliases `-c` for `--config` and `-k` for `--check-config`.
- `full`, `view`, and `silent` check modes with `print_config_check()`. Full mode
  reports the actual config lookup and final normalized JSON; view prints only
  JSON, and silent prints only `OK`. All example applications use these helpers.

### Changed

- Move repository-specific tools into `tools/json_config/` and update CMake,
  tests, and documentation to use their new paths.
- Rework the README for new users and add a dedicated introduction covering
  application configuration, build collection, C++ mapping, and structured configs.
- Bare `--check-config` now selects full output in the examples. The existing
  boolean check flag and loader API remain available; applications choose when
  to print help/check output and exit. The parser now exposes help through `.help`.

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
