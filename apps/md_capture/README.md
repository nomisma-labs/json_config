# md_capture

This is the runnable counterpart of the [complete example in jcfg.md](../../docs/json_config/jcfg.md#complete-example).
It demonstrates a nested market-data capture configuration, inheritance, donor
imports, fragment imports, deletion, and replacement. The sample simulates startup
by printing the resolved capture settings after mapping and validation.

The guide and application share the five inputs in [config/](config/).
Expected generated JSON stays in the guide's
[complete example outputs](../../docs/json_config/examples/complete_config/genconfig/).
For the full build, test, packaging, and installation workflow, see
[config_workflow_example](../config_workflow_example/README.md).

## Build and run

Run these commands from the repository root after installing the
[toolchain](../../docs/kit/build.md#toolchain):

```bash
cmake --preset dev
cmake --build --preset dev --target json_config_md_capture
build/dev/bin/md_capture --config build/dev/config/md_capture/md_capture.json
build/dev/bin/md_capture --config build/dev/genconfig/prod/md_capture.json
build/dev/bin/md_capture --config build/dev/genconfig/prod-liquidation/md_capture_liquidation.json
```

The app build collects the config inputs and generates all four sections because
its `nom_jcfg_generate()` registration uses `BUILD_WITH_APP`. The ordinary
`md_capture.json` can also be loaded directly, without generation.

| Section | Connection | Subscription | Logging |
|---|---|---|---|
| `base` | Development endpoints, timeout 5 | `btcusdt`; trade and depth | Debug, console |
| `ci` | Shared current endpoints, timeout 20 | Inherited development subscription | Warn, deployment log and data paths |
| `prod` | Inherited from `ci` | Imported spot symbols: `btcusdt`, `ethusdt` | Inherited from `ci` |
| `prod-liquidation` | Replaced with the development connection | Replaced with liquidation symbols and channel | Log file deleted; separate data path |

The last section generates `md_capture_liquidation.json`, so it is selected
explicitly above. For sections producing `md_capture.json`, lookup can use the app's
config key:

```bash
NOM_CONFIG_DIR=../genconfig/prod build/dev/bin/md_capture --check-config
```

The search path is relative to the executable directory. See
[app/config binding](../../docs/json_config/app_config_binding.md) for lookup rules.

## C++ mapping and validation

[main.cpp](main.cpp) declares nested config structures with common `cfg_*` types.
`NOM_JCONFIG` maps the root and nested structures using the same snake_case
names in JSON and C++.

The four root components are required. `connection.timeout_s` defaults to 5 if
omitted, and `log.file` is optional: deleting that key selects console output.
Each structure's `validate()` checks its own fields and delegates validation of
nested structures to them. It returns `std::expected<void, std::string>`; parents
add the child's JSON path to errors so failures identify the full config location.

`main()` parses config arguments, loads/maps/validates the configuration, and
reports its source. `--check-config` exits at that point. Normal execution prints
the settings that would initialize capture, including symbols, channels, and paths.

Runtime overrides use the same JSON paths as the guide:

```bash
build/dev/bin/md_capture --config build/dev/genconfig/prod/md_capture.json \
    --set /connection/timeout_s=11 --set /log/level=info
build/dev/bin/md_capture --config build/dev/genconfig/prod/md_capture.json \
    --set '/subscription/symbols=[]' --check-config
```

The first command changes the settings used at startup. The second fails
validation because an empty symbol list is not a usable capture subscription.

## Tests

[CMakeLists.txt](CMakeLists.txt) registers one `nom_jcfg_test()` per section.
After building the `complete` preset, run its checks with the command below; a `dev` build
leaves integration tests out and runs only the four section checks:

```bash
ctest --preset complete -R '^json_config_md_capture_'
```

Four tests run `--check-config` on the generated sections. A CLI suite compares
generated JSON with the documented outputs and checks the C++ values used by the
application, including deletion and replacement in `prod-liquidation`. It also
checks direct loading, nested overrides, startup suppression, and mapping and
validation failures. The generator engine and CLI tests independently verify the
same source inputs against the documented outputs.
