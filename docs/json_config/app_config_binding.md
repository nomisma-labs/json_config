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

The caller can supply the resolved executable path. A convenience
adapter may obtain it through a library-local OS utility; `argv[0]` alone may be a
bare command name. Keep OS-specific executable discovery behind that utility,
with a future move to a shared `core/util/os.h` facility.

When two or more variants of an app (e.g. `mm_strategy` and `mm_strategy_perf`)
use the same config (`mm_strategy.json`), they can override `<key>` for automatic
config resolution:

```cmake
nom_app(mm_strategy_perf SOURCES main.cpp COMPILE_OPTIONS -DNOM_CONFIG_KEY=mm_strategy)
```

The application stringifies `NOM_CONFIG_KEY` and passes it through the config
library's config-key override parameter. Explicit `--config` and `NOM_CONFIG`
selections retain their precedence.

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

The public C++ entry point belongs in the `nom.json_config` module. Exact function
and option type names will be defined during implementation.

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
