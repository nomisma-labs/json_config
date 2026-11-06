# App/Config binding

The locator chooses a JSON file. The loader parses it, applies runtime
substitution and overrides, then maps and validates the application config.
C++ does not interpret jcfg inheritance, imports, delete, or override instructions.

## Config resolution

An application has a config lookup key, normally its executable basename, with an
optional explicit key supplied by the application. Automatic file lookup uses
`<key>.json`. If `jcfg.gen_file` chooses another filename, select that file
explicitly; the locator does not read jcfg metadata to infer it.

Lookup order:

| Priority | Source | Meaning | Value |
|---|---|---|---|
| 1 | `--config FILE`| Explicit selection of config file at the app start | Config file path |
| 2 | `NOM_CONFIG` env variable | Explicit selection of config set via process env  | Config file path |
| 3 | `<app-directory>/<key>.json` beside the executable | Config bundled together with the app |  |
| 4 | `NOM_CONFIG_DIR` env variable | One or more config search paths | Colon-separated list of dirs in order, as described below. |
| 5 | Empty JSON object `{}` | Last resort: map to C++ using optional or defaulted values | Used only if config lookup finds no file. |

`NOM_CONFIG_DIR` is a colon-separated directory list on the supported Unix
platforms. In each directory, try `<key>.json`, then `<key>/<key>.json`. Relative
entries are resolved against the executable directory. Relative explicit filenames
from `--config` or `NOM_CONFIG` are resolved against the process working directory.
An empty explicit filename or empty directory-list entry is an error.

Explicit `--config` and `NOM_CONFIG` selections are authoritative: a missing or
unreadable file fails immediately. During directory search, a missing candidate
allows the next candidate; an existing unusable candidate is an error. After a
file is selected, parsing or validation failure does not trigger another search.

For example, with a binary at `build/dev/bin/md_capture`:

```bash
build/dev/bin/md_capture --config build/dev/genconfig/prod/md_capture.json
NOM_CONFIG_DIR=../genconfig/prod build/dev/bin/md_capture
NOM_CONFIG_DIR=../config build/dev/bin/md_capture
```

The last command finds the collected default at `config/md_capture/md_capture.json`.
The locator does not automatically choose between `dev`, `ci`, and `prod` sections.

The caller can supply the resolved executable path through
`config_options::executable_path`. Otherwise, `current_executable_path()` obtains
it through a library-local OS utility; `argv[0]` alone may be a bare command name.
The private boundary is `core/json_config/util/os.h`, with its Linux implementation
selected by CMake. When a common core OS library is available, move that operation
there and keep the config locator calling the same boundary.

When two or more variants of an app (e.g. `mm_strategy` and `mm_strategy_perf`)
use the same config (`mm_strategy.json`), they can override `<key>` for automatic
config resolution:

```cmake
nom_app(mm_strategy_perf SOURCES main.cpp COMPILE_OPTIONS -DNOM_CONFIG_KEY=mm_strategy)
```

`<json_config/config.h>` supplies `NOM_CONFIG_KEY_VALUE`, which stringifies the
application's `NOM_CONFIG_KEY` definition (or expands to an empty string when it is
not defined). Pass it to `parse_config_arguments()` or set
`config_options::config_key` directly. Explicit `--config` and `NOM_CONFIG`
selections retain their precedence.

### Selecting search directories in Bash

For convenience, the framework provides a set of bash functions that help to manage search config path.

Source [config_path.sh](../../tools/json_config/config_path.sh) from
`~/.bash_aliases`, using the absolute path to your checkout:

```bash
source /path/to/json_config/tools/json_config/config_path.sh
```

If `~/.bashrc` already loads `~/.bash_aliases`, new Bash sessions will have the
commands available. Run `source ~/.bash_aliases` to load them in an existing shell.
Sourcing the script defines the commands without changing the current selection.

| Command | Effect |
|---|---|
| `cfg_set` | Replace `NOM_CONFIG_DIR` with the current directory. |
| `cfg_add` | Append the current directory unless it is already listed; start a new list if unset or empty. |
| `cfg_clear` | Unset `NOM_CONFIG_DIR` and print the cleared state. |
| `cfg_print` | Print one search directory per line, in order; report if unset or empty. |

Both `cfg_set` and `cfg_add` print the resulting search path after a successful
call, using the same format as `cfg_print`, including when a directory is already listed.

```bash
cd /srv/config/prod
cfg_set
# /srv/config/prod
cd /srv/config/shared
cfg_add
# /srv/config/prod
# /srv/config/shared
cfg_print
# /srv/config/prod
# /srv/config/shared
cfg_clear
# NOM_CONFIG_DIR is not set
```

All commands take no arguments. The setters export absolute physical paths
(resolving directory symlinks), so later `cd` commands do not change the selection.
Repeated additions preserve the existing order. Spaces in paths are supported;
colons are rejected because they separate entries. `cfg_add` rejects an existing
list with empty entries; `cfg_set` can replace it.
`cfg_clear` unsets the variable because an empty directory list is invalid for the locator.

The selection applies to this shell and programs subsequently launched from it.
Other existing terminals are unaffected. Explicit `--config`, `NOM_CONFIG`, and
config files beside the executable still take precedence over this directory list.

## Loading and overrides

After config resolution, the loader:

1. Reads the selected file and parses its JSON object, or uses `{}` if lookup found
   no file (see [hard-wired config](#hard-wired-config)).
2. Maps the object to the application's [C++ config types](json_cpp_mapping.md).
3. Calls the root config's `validate()` protocol. On success, the config is ready for application initialization.

Config structures support `validate()` returning `std::expected<void, std::string>`:
success has no value; failure carries an error message. See [validation](json_cpp_mapping.md#validation-boundaries).

A read, parse, mapping, or validation error stops config initialization.

### Runtime overrides

Runtime overrides modify the JSON before final C++ mapping and validation.
For hard-wired config, the override targets come from serializing the initialized
C++ config, as described [below](#hard-wired-config).
First substitute `{jcfg:run-var NAME}` placeholders, then apply
`--set <JSON-pointer>=<value>` arguments in command-line order, as described in
[runtime value overrides](jcfg.md#runtime-value-overrides).

Every stage must succeed before the next begins. A missing environment variable
fails before CLI overrides, even if an override would replace that property.
Missing override targets are errors. Repeated overrides of the same target apply
in order, with the last successful assignment taking precedence.

The loader selects the most appropriate data type for each provided override value.
Each override must map successfully before the next is attempted; a later override
cannot repair another malformed field that prevents the current mapping probe.
Application validation runs only after all overrides have been applied.

```bash
build/dev/bin/md_capture --config build/dev/genconfig/prod/md_capture.json \
    --set /connection/timeout_s=20 --set /log/level=debug
```

## Application integration

Keep location, JSON loading, and [typed mapping](json_cpp_mapping.md) available
separately. The core API accepts parsed options; an optional command-line adapter
handles `--config`, repeated `--set`, and `--check-config` alongside app-owned options.
It must not consume unrelated application arguments.

Return the selected path and selection source with the load result, so the app can
report which config it used. For hard-wired config, report that source without a
file path. Failures identify the loading stage and relevant
file or JSON Pointer. The library reports errors to its caller; the application's
entry point chooses logging and exit behavior. No logger or assembly dependency is
required to load config.

`--check-config` runs the same loading and validation steps, reports success
or failure, and exits before starting application services. Successful output can
identify the selected file without dumping config values. Detailed printing and
its treatment of sensitive fields remain a separate interface decision.

Import `nom.json_config` and link `nom::json_config`. The main entry point is
`load_config<Config>(options)`, returning `loaded_config<Config>` with `.value`
and `.location` (`.path` and `.source`). The root type must provide
`validate() const` returning `std::expected<void, std::string>`.

```cpp
const auto arguments = nom::jcfg::parse_config_arguments(argc, argv, NOM_CONFIG_KEY_VALUE);
const auto loaded = nom::jcfg::load_config<app_config>(arguments.options);
```

The adapter returns app-owned arguments in `.remaining_arguments`, preserves `--`
and everything following it, and accepts `--config` once. It uses
Boost.Program_options with exact long option names; `--config=FILE` and
`--set=POINTER=value` are also accepted. The application checks
`.options.check_config` to exit after successful loading, before starting services.
See the [complete application example](../../apps/config_workflow_example/README.md).

The [application-argument example](../../apps/config_args_example/README.md) passes
`.remaining_arguments` to its own Boost.Program_options parser before calling
`load_config()`. Handle application help and argument validation at this point,
so help does not require a readable config and invalid app arguments fail before
config loading. Both parsers and the application startup remain controlled by
`main()`.

| Function | Purpose |
|---|---|
| `locate_config(options)` | Choose a file or the hard-wired source. |
| `read_config(location)` | Read a JSON object, or return `{}` for the hard-wired source. |
| `substitute_runtime_variables(document)` | Expand runtime placeholders in JSON values. |
| `apply_config_overrides(document, overrides, probe)` | Apply ordered overrides using a caller-supplied mapping check. |
| `load_config<Config>(options)` | Run the full sequence, native nlohmann mapping, and root validation. |

Location, substitution, and typed loading accept an optional `environment_lookup`
callback; by default they read the process environment. Mapping checks use native
nlohmann conversion, for example `document.get<Config>()`, independently of the
locator. `config_error` reports `.stage`, `.file`, `.pointer`, and an explanatory
`what()` message. Nlohmann mapping errors retain their diagnostic JSON paths.

## Hard-wired config

If config resolution exhausts its lookup steps without finding a file, the loader
provides an empty JSON object `{}` as a last resort and tries to map it to C++.
Successful mapping means the config can be initialized entirely from optional or
defaulted values; this is a **hard-wired config**. Missing required values cause
mapping to fail. The resulting C++ config must still pass `validate()`.

Runtime command-line overrides can still be applied to hard-wired config. After
successful mapping of `{}`, serialize the initialized C++ config to JSON and apply
the overrides to that object's values. Map the updated JSON back to C++, then
validate the final config.

This fallback applies only when lookup finds no file without an error. It does not
rescue a broken explicit path, an unreadable or malformed file, or failed mapping
or validation.
