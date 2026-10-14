# config_example

This application demonstrates JSON/C++ mapping, config lookup and loading,
validation, command-line overrides, and generated config tests. Startup remains
in [main.cpp](main.cpp): parse config arguments, load and validate the config,
then initialize the application or exit through `--check-config`.

## Build

Run all commands in this README from the repository root. Follow the shared
[toolchain requirements](../../docs/kit/build.md#toolchain), then:

```bash
cmake --preset dev
cmake --build --preset dev --target json_config_config_example
```

The executable is `build/dev/bin/config_example`. Its config directory is
collected into `build/dev/config/config_example/`. The build also generates
`build/dev/genconfig/dev/config_example.json` and
`build/dev/genconfig/prod/config_example.json`.

## Config structure

[main.cpp](main.cpp) defines `app_config`, maps its members using `NOM_CONFIG`,
and implements the root `validate()` protocol.

| Field | C++ type | Default / constraint |
|---|---|---|
| `endpoint` | `defaulted<std::string>` | `https://example.test/feed`; must be nonempty |
| `timeout_s` | `defaulted<jint>` | `10`; must be positive |
| `enabled` | `defaulted<bool>` | `true` |
| `interface_name` | `std::optional<std::string>` | Absent by default |

The types are available through `nom::jcfg::types`. All fields are defaulted or
optional, so loading an empty JSON object succeeds. If config lookup finds no
file, these C++ defaults also support hard-wired initialization.

The ordinary [config_example.json](config/config_example.json) supplies the three
defaulted values. The simulated application prints the timeout after initialization;
`enabled` and `interface_name` illustrate mapping and are not used by its behavior.

## Run

Select the collected default through a search directory:

```bash
NOM_CONFIG_DIR=../config build/dev/bin/config_example
```

`NOM_CONFIG_DIR` entries are resolved against the executable directory, so
`../config` selects `build/dev/config`. The app reports the selected source and
absolute path, then prints `Application initialized; timeout: 10 seconds`.

Select a file explicitly and override its timeout:

```bash
build/dev/bin/config_example \
    --config build/dev/config/config_example/config_example.json \
    --set /timeout_s=20 --check-config
```

This reports `Config OK` with the selected source/path and exits successfully
before application initialization. Without `--check-config`, it also prints the
initialization message with a timeout of 20 seconds.

| Argument | Meaning |
|---|---|
| `--config FILE` | Select one JSON file; relative paths use the working directory |
| `--set POINTER=value` | Override an existing config value; repeatable, applied in order |
| `--check-config` | Load, map, validate, and exit before initialization |

Adjacent forms such as `--config=FILE` and `--set=/timeout_s=20` are also accepted.
Unrecognized arguments print usage and exit with status 1. Config errors print
their loading stage and exit with status 1; for example, setting `/timeout_s=0`
fails root validation. The app has no application-specific options or help flag.
See [app/config binding](../../docs/json_config/app_config_binding.md) for lookup
precedence, environment selection, runtime variables, and override rules.

## Generated sections and CMake tests

The [jcfg source](config/config_example_jcfg.json) defines `dev`, which imports
the ordinary JSON file, and `prod`, which extends `dev` and changes `endpoint` to
`https://example.test/production-feed` and `timeout_s` to `30`.

[CMakeLists.txt](CMakeLists.txt) registers generation and generated-config tests:

```cmake
nom_jcfg_generate(config_example SOURCE config_example_jcfg.json BUILD_WITH_APP)
nom_jcfg_test(config_example dev)
nom_jcfg_test(config_example prod)
```

The helpers check the project's options internally: generation requires
`nom_build_apps`, and tests require both `nom_build_tests` and `nom_build_apps`.
The separate override test keeps its guard because it uses `add_test()` directly.

Omitting `SECTIONS` generates all enabled sections. `BUILD_WITH_APP` prepares the
generated files whenever this app is built. Select a generated section explicitly:

```bash
build/dev/bin/config_example --config build/dev/genconfig/prod/config_example.json
```

It reports the selected config and initializes with a timeout of 30 seconds.
Generation can also be requested separately:

```bash
cmake --build --preset dev --target json_config_generate_jcfg_config_example
```

When tests are enabled, run all three checks with:

```bash
ctest --preset dev -R '^json_config_config_example_'
```

Two `nom_jcfg_test()` checks exercise the real loader, mapping, and validation for
`dev` and `prod`. A separate integration test loads the collected ordinary JSON,
applies `/timeout_s=20`, and uses `--check-config`. CTest expects these files and
the executable to have been built first. See
[build-system integration](../../docs/json_config/build_system_integration.md)
for the helper API, aggregate config targets, cleanup, and installation.
