# config_args_example

This application demonstrates config arguments combined with application-owned
arguments. The framework parser extracts config options and leaves the remaining
arguments for the application's Boost.Program_options parser. Startup stays in
[main.cpp](main.cpp): parse config arguments, parse application arguments and
handle help, load and validate the config, then check or run the application.

## Build

Run all commands in this README from the repository root. Follow the shared
[toolchain requirements](../../docs/kit/build.md#toolchain), then:

```bash
cmake --preset dev
cmake --build --preset dev --target json_config_config_args_example
```

The executable is `build/dev/bin/config_args_example`. Its
[JSON config](config/config_args_example.json) is collected into
`build/dev/config/config_args_example/config_args_example.json`. This example
uses an ordinary JSON file and does not register jcfg generation.

## Config and application arguments

The C++ config has one field, `greeting`, of type
`nom::cfg_defaulted<nom::cfg_string>`, with a default of `Hello`. `NOM_JCONFIG` maps
it to JSON, and root `validate()` requires it to be nonempty. The collected JSON
also contains `"greeting": "Hello"`. If lookup finds no config file, the C++
default permits hard-wired initialization.

[app_arguments.cpp](app_arguments.cpp) defines the application parser, with its
result declared in [app_arguments.h](app_arguments.h).

| Application argument | Meaning |
|---|---|
| `--name NAME` or one positional name | Greeting recipient; defaults to `world`, must be nonempty |
| `--count N` | Positive integer number of greetings; defaults to `1` |
| `--verbose`, `-v` | Print the greeting count before running |
| `--help`, `-h` | Print application and config help, then exit before config loading |

| Framework argument | Meaning |
|---|---|
| `--config FILE` | Select one JSON file; relative paths use the working directory |
| `--set POINTER=value` | Override an existing config value; repeatable, applied in order |
| `--check-config` | Load, map, validate, print `Config OK`, and exit before greetings |

Adjacent values such as `--name=Alex`, `--config=FILE`, and `--set=/greeting=Hi`
are accepted. Unrecognized application options, extra positional names, invalid
counts, and empty names fail during application argument processing, before config
loading. Config parsing, mapping, or validation failures stop startup. Errors are
printed to standard error and return status 1; success returns 0.

## Run

Select the collected config and use the application defaults:

```bash
NOM_CONFIG_DIR=../config build/dev/bin/config_args_example
```

This prints `Hello, world!`. Search-directory entries are resolved against the
executable directory, so `../config` selects `build/dev/config`.

Config and application options may be interleaved:

```bash
build/dev/bin/config_args_example --name Alex \
    --config build/dev/config/config_args_example/config_args_example.json \
    --count 2 --set /greeting=Hi --verbose
```

Output:

```text
Greeting count: 2
Hi, Alex!
Hi, Alex!
```

A positional name can follow short application flags:

```bash
NOM_CONFIG_DIR=../config build/dev/bin/config_args_example -v Alex
```

This reports `Greeting count: 1`, then prints `Hello, Alex!`.
The `--` marker ends option processing in both parsers; a config-looking argument
after it becomes the positional name:

```bash
build/dev/bin/config_args_example \
    --config build/dev/config/config_args_example/config_args_example.json \
    -- --set
```

This prints `Hello, --set!`.

Help does not open or validate the selected file:

```bash
build/dev/bin/config_args_example --config missing.json --help
```

This prints help and succeeds even though `missing.json` does not exist. Framework
argument syntax is still parsed first, so malformed config options can fail before
help is reached.

Check mode validates both argument sets and the config without printing greetings:

```bash
build/dev/bin/config_args_example \
    --config build/dev/config/config_args_example/config_args_example.json \
    --name Alex --count 2 --verbose --set /greeting=Hi --check-config
```

This prints only `Config OK`. See
[app/config binding](../../docs/json_config/app_config_binding.md) for lookup
precedence, environment selection, runtime variables, and override rules.

## Tests

After building the `complete` preset, run its CLI integration suite; a `dev` build leaves
integration tests out:

```bash
ctest --preset complete -R '^json_config_config_args_example_cli$'
```

The [suite](../../tests/integration/config_args/cli_test.py) contains nine Python
unittest methods covering defaults, interleaved and adjacent options, positional
names and short flags, `--` preservation, help before config I/O, application
errors before loading, check mode, and framework diagnostics. It uses temporary
config files and runs the built executable through CTest.
