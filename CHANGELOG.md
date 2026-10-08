# Changelog

Notable changes to this repo. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org); how to release:
[docs/kit/build.md](docs/kit/build.md#versioning).
A release's `### Upgrade` section lists the steps derived repos take by hand; `tools/kit/sync_template.py` prints them. <!-- template-only -->

## [Unreleased]

## [0.6.0] - 2026-10-05

### Changed

- The shared docs move to `docs/kit/`, next to `cmake/kit/` and `tools/kit/`: `build.md`,
  `conventions.md`, `howto.md` and `new_repo.md`, with an index in `docs/kit/README.md`. A repo
  keeps its own docs in `docs/`, outside the kit.
- `AGENTS.md` splits in two. The rules every repo shares move to the template-owned
  `docs/kit/agents.md`; a repo's `AGENTS.md` keeps its description and its own rules, and imports
  the shared file with `@docs/kit/agents.md`.
- The shared CMake presets move to `cmake/kit/presets.json`. A repo's `CMakePresets.json`
  includes that file and holds only presets of its own.
- `cmake/README.md` moves into the kit as `cmake/kit/README.md`.
- `README.md` drops its bootstrap and manual build sections for a short build section; their
  details are in `docs/kit/build.md`.

### Upgrade

- Shorten `AGENTS.md` to the repo's description, the line
  `Read and follow the rules every Nomisma repo shares: @docs/kit/agents.md`, and the repo's own
  rules; `docs/kit/agents.md` now has the shared sections.
- Replace `CMakePresets.json` with cmake_template's, which includes `cmake/kit/presets.json`, and
  keep only presets of the repo's own in it, under names the kit doesn't use.
- Delete `cmake/README.md` unless it describes the repo's own CMake files; the kit's README is now
  `cmake/kit/README.md`.
- Point `docs/README.md` at `docs/kit/README.md`, and replace the bootstrap and manual build
  sections of `README.md` with a short build section that links `docs/kit/build.md`.
- Update the links the sync lists, and the `docs/build.md` link at the top of `CHANGELOG.md`: the
  shared docs moved from `docs/` to `docs/kit/`.

## [0.5.0] - 2026-10-05

### Changed

- Template-owned tools move to `tools/kit/`, which repos receive as a whole; a repo keeps its own
  tools in `tools/`, outside the kit. The checks drop their `check_` prefix and live in
  `tools/kit/checks/` (`tools/check_format.sh` is now `tools/kit/checks/format.sh`), and
  `tools/kit/README.md` lists every tool.
- `tools/kit/sync_template.py` replaces template-owned files instead of merging differences into
  them. It syncs to the newest release by default and runs the target release's copy of itself.
  When a template-owned file was edited in the repo it changes nothing and lists the file, and
  `--overwrite` takes the template's version. It stages `.template_ref` with the rest, then lists
  the repo's own files that mention moved or removed template paths, repo files inside the kit's
  folders, and the upgrade steps of the releases in between.

### Added

- `### Upgrade` sections in this changelog for the steps derived repos take by hand.
- An integration test of the sync on a scratch template and scratch repos.

### Upgrade

- A repo created before 0.5.0 has the old `tools/sync_template.py`, which can't sync to this
  release. Run the new script once from a `cmake_template` checkout at v0.5.0 or later, next to
  the repo: `python3 ../cmake_template/tools/kit/sync_template.py`. Later syncs run the repo's
  own `tools/kit/sync_template.py`.
- Update the repo's own files that the sync lists: tool paths moved from `tools/` to `tools/kit/`.

## [0.4.1] - 2026-10-05

### Fixed

- Keep template-only content out of template-owned files, which reach every derived repo: the
  example wheel job moves from `ci.yml` to the template-only `ci-template` workflow, the example's
  wheel checks move from `docs/build.md` to the README, and `bootstrap.sh` no longer looks for
  `tools/init_project.py`. A `ci-template` job fails when template-owned files mention
  template-only names.
- `docs/build.md` describes the CI that runs: the `ci` workflow preset and the first four checks.

## [0.4.0] - 2026-10-04

### Changed

- Organize the CMake kit into client helpers (`api/`), dependency services (`deps/`), internal
  utilities and package templates. Keep `nom_kit.cmake` and existing helper names as the stable
  interface; document the layout and common helper usage in `cmake/README.md`.
- Advance the shared kit identifier to `0.4.0`.

### Added

- Optional Linux Python wheels through scikit-build-core, with shared `nom_python_module()`
  installation rules and pinned packaging tools kept separate from ordinary C++ builds.
- An example Python package using the native module API and a bundled shared library, with
  installed-wheel checks for source-built and imported C++ libraries. Repository initialization
  removes the example while retaining reusable packaging support.

## [0.3.0] - 2026-10-03

### Added

- Metal v2.1.4 in the common dependency baseline. Generated repositories retain the pinned
  submodule and use `nom_find_metal()` and `nom::metal` from the shared kit. Installed packages
  share the `nom_metal` package, headers and license.

### Changed

- Include naming checks in bootstrap runs that select `clang`.
- Define Nomisma library APIs in native named modules. Keep headers for macros and
  third-party integration.
- Convert `nom.example` to module interfaces, partitions and implementation units. The `hello`
  app, unit tests and both source and installed-package consumers import the same API.
- Update the shared recipes and conventions so new repositories start with modules.
- Advance the shared kit identifier to `0.3.0`.

## [0.2.0] - 2026-10-03

### Added

- Explicit `STATIC` and `SHARED` linkage options for `nom_lib()`, allowing mixed library
  linkage without changing the global `BUILD_SHARED_LIBS` setting.
- Relocatable Unix installations: installed applications and shared libraries find packaged
  libraries relative to their own location, with an integration test that moves the install prefix.

### Changed

- GCC-based presets use `g++-16`, with GCC 16 visible in their display names.
- The CI image and hosted runners use Ubuntu 26.04. The image uses Ubuntu's compiler and build-tool
  packages instead of the toolchain PPA and Kitware repository.
- The CI image includes the Boost, OpenSSL, fmt, zlib and Cap'n Proto development dependencies;
  the removed Boost.System stub package is no longer requested.
- Advance the shared kit identifier to `0.2.0` so mixed copies report the updated kit's version.

## [0.1.0] - 2026-10-02

### Fixed

- Leave `.template_ref` blank in the unhosted template and require a resolvable template
  commit before initializing a new repository.

### Added

- Use `*_test.cxx` for C++ unit and integration test sources so they can be selected separately
  from regular `*.cpp` sources; include `.cxx` in formatting and SPDX checks.

- Python environment check in bootstrap and CI: Python 3.10+, repository script syntax, and
  imported modules. Python-dependent steps are skipped on failure.

- Optional preset argument for `bootstrap.sh`, with available presets discovered through CMake
  and listed in `--help`. The default selects all matching configure/build/test names except
  `release`; cleanup is limited to the selected builds. CMake checks each preset's toolchain.

- `nom_app(... BUILD_FOR ...)` selects normal build/install participation; `any` is the default.
  The `complete` preset is a Debug build with every app, including sandbox experiments;
  bootstrap covers it. Excluded apps remain explicitly buildable.

- Discover Catch2 and enable testing at the project root before code directories, supporting
  both top-level and inline unit test suites.

- Separate `tests/unit/` and `tests/integration/`, with CTest labels for selecting either group;
  rename the library usage integration helper to `nom_library_usage_tests()`.

- `nom_app()` collects an adjacent `config/` directory into the build tree and installs it with the app executable.
- Name target helpers `nom_lib`, `nom_app` and `nom_c2test`, keeping the testing framework explicit.
- `bootstrap.sh`: clears prior validation builds/logs, initializes dependencies and runs fresh
  preset builds/tests and quality checks, with per-step
  results, logs, a final summary and next steps; included in template synchronization.
- The template: the CMake kit, presets, checks, CI and the CI image, agent files, and the recipes
  for adding code (`docs/howto.md`).
- The example code (library `example`, app `hello`, their tests, the library usage test project) and the lab.
- `tools/init_project.py`, which turns a copy of the template into an empty repo, and
  `tools/sync_template.py`, which keeps derived repos in step with the template.
