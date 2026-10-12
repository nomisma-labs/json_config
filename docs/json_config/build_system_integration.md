# Build system integration

The shared CMake kit handles config collection. `json_config` defines generation,
config targets, test registration, and installation of generated configs.

## Collection

An application declared with `nom_app()` automatically collects its adjacent
`config/` directory. For example, in `apps/md/md_capture/CMakeLists.txt`:

```cmake
nom_app(md_capture SOURCES main.cpp BUILD_FOR dev ci)
```

The [kit collector](../../cmake/kit/api/targets.cmake) and config generator use these locations:

| Source or artifact | Build location |
|---|---|
| Executable | `build/<preset>/bin/md_capture` |
| `apps/md/md_capture/config/` | `build/<preset>/config/md_capture/` |
| Generated `prod` config | `build/<preset>/genconfig/prod/md_capture.json` |

Collection refreshes the destination whenever the app builds, including removing
files deleted from the source directory. Executables declared in the same source
directory share one collection target. App directory basenames must be unique in
the combined build because they share the collection root.

Collection copies files without interpreting them. It remains useful for an app
that loads an ordinary JSON config directly and never runs the generator.

Donor files outside the app folder need explicit locations or collection into a
matching directory layout. Copying only the app folder does not preserve imports
such as `../shared/settings.json`. For shared config libraries, the build can supply
`NOM_CONFIG_LIB` in the generator's environment, pointing to an absolute project
directory. Imports reference it as `{jcfg:gen-var NOM_CONFIG_LIB}`; see
[common config libraries](jcfg.md#advanced-jcfg-generation-topics).

## Registering generation

The build preset selects applications and compiler settings; a jcfg section
selects application configuration. For example, a `dev` build may generate `prod`
configs. These selections must not be implicitly tied together.

The `json_config` CMake helper registers:

| Setting | Example |
|---|---|
| App name | `md_capture`, already declared by `nom_app()` |
| Source relative to its config directory | `md_capture_jcfg.json` |
| Sections to generate and install | `ci`, `prod` |
| Generator command | Discovered Python interpreter plus `jcfggen.py` |

Select sections explicitly. Resolve output filenames from jcfg metadata through
the generator's [listing operation](config_generation.md#command-line),
so the app's CMake file does not repeat `jcfg.gen_file`. A change to section names
or output filenames must refresh that output plan.

Config-generation CMake scripts and functions belong in `json_config`, including
registration, build dependencies, and installation of generated outputs.
`cmake_template` provides the existing config collector and remains unaware of
generation. Client repositories use the helpers supplied by `json_config` and
declare their app settings. The helper's exact CMake signature will be defined
during implementation.

## Build sequence

1. Collect the app's source configs.
2. Run the generator against the collected source, with explicit input/output paths.
3. Check that every requested section produced its expected JSON object.
4. Record the successful output set, then allow the app build target to complete.

The generation target depends on collection; the app target depends on generation.
Several executables sharing the same source and selection share one generation
operation. Conflicting registrations or overlapping output ownership are errors.

Initially, run generation after collection on every requested app build. This
observes donor-file changes, removed files, and generation-time environment
changes. Unchanged output contents need not be rewritten, and config-only changes
do not require recompiling C++. CMake discovers the Python interpreter and passes
absolute tool paths so execution does not depend on the working directory.

Maintain a build record of the generated files and their content hashes. Invalidate
it before a new attempt, and mark it successful only after generation completes.
Remove renamed or deselected files owned by that registration without touching
other apps' outputs. Valid runtime `{jcfg:run-var NAME}` placeholders are allowed in
generated files; C++ type and application validation happen when the app loads them.

## Repository-wide config target

The `config` target runs all config collection and generation tasks
registered in the current build tree. It includes apps that only collect ordinary
JSON files and apps whose executables are excluded from the normal build by
`BUILD_FOR`. Generation uses the sections selected in each app's registration.

```bash
ninja -C build/dev config
```

The `json_config` CMake helpers provide this aggregate target. It depends directly
on the collection and generation targets, so it can prepare configs without
compiling applications. Generation follows its input collection, shared operations
run once, and any collection or generation error fails the target.

## Testing generated configs

`json_config` defines an opt-in CMake helper,
`config_test(app_name config_section)`, for registering config checks with CTest:

```cmake
config_test(md_capture prod)
```

Each call registers one test that runs the app with the section's generated config
using `--config FILE --check-config`. It exercises config loading, JSON/C++ mapping,
and validation, then exits before starting application services; see
[app integration](app_config_binding.md#application-integration).
Build the app and generate its config before running CTest. Tests are registered
only for explicitly requested app/section pairs, allowing projects to control
the size of their test suite.

## Cleaning build configs

The `clean_config` target removes all collected and generated configs
for the current build tree, including current outputs and files left by renamed
or removed apps or sections. It deletes both `build/<preset>/config/` and
`build/<preset>/genconfig/`, together with generation success records, without
needing to inspect the current jcfg definitions. Original source configs remain
intact. Run `ninja config` to collect and generate all registered configs again,
or build an app to restore its configs.

```bash
ninja -C build/dev clean_config
```

The same cleanup is included in `ninja clean`. The `json_config` CMake helpers
register the cleanup target and paths independently of individual app registrations,
so cleanup remains available even after the last generation registration is removed.
Normal clean integration uses CMake's
[ADDITIONAL_CLEAN_FILES](https://cmake.org/cmake/help/latest/prop_dir/ADDITIONAL_CLEAN_FILES.html)
directory property.

## Installation and deployment

When installation is enabled, the current kit installs selected executables under
`<prefix>/bin/` and their collected sources under `<prefix>/config/<app-directory>/`.
The generation integration additionally installs its selected outputs under
`<prefix>/genconfig/<section>/<gen_file>`.

Build before installing. Installation must reject a missing success record,
missing output, or output changed since successful generation. An app excluded by
`BUILD_FOR` remains explicitly buildable but is not installed. A shared config
group is installed when one of its apps is selected.

Generated JSON is runtime data; running the application does not require Python
or access to donor files. Deployment can select a generated file explicitly with
`--config`, or expose a section directory through `NOM_CONFIG_DIR`, as described
in [app/config binding](app_config_binding.md). Installing into an existing prefix
does not remove files from older installations; use a fresh staging prefix when
checking the intended deployment contents.
