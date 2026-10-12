# json_config

<!-- repo-description:begin -->
**json_config** is Nomisma's application configuration library based on JSON.
It shares its design with [toml_config](https://github.com/nomisma-labs/toml_config),
with similar capabilities planned for both format implementations.
<!-- repo-description:end -->

The scope includes layered configuration generation with a Python tool
(`jcfggen.py`), C++ config resolution, JSON/C++ mapping, and build-system
integration for collecting and generating app configs.

## Implementation status

The [configuration documentation](docs/json_config/README.md) defines the library's
behavior. The [Python generator engine](tools/jcfg.py) resolves jcfg sections to
application JSON in memory. The [jcfggen.py CLI](tools/jcfggen.py) lists metadata
and writes generated configs, preserving unchanged output files. Both use Python
3.10+ and the standard library; no package installation is needed.

The [CMake helpers](docs/json_config/build_system_integration.md) register generation,
config tests and installation, and provide `config` and `clean_config` targets.
App builds collect configs; generation is requested with `config` for the whole
build or `<project>_generate_jcfg_<app>` for one app, or opted into with
`jcfg_generate(... BUILD_WITH_APP)`. Omit `SECTIONS` to generate all sections with
generation enabled. The helpers reuse the existing
`cmake_template` config collector.

The [JSON/C++ mapping library](docs/json_config/json_cpp_mapping.md) is available
through `import nom.json_config;` and the CMake target `nom::json_config`.
`NOM_CONFIG` in `<json_config/config.h>` maps application structures with required,
defaulted, and optional members using nlohmann/json. It enforces supported config
types, including checked numeric types `jint`, `juint`, and `jdouble`.
These types and `defaulted<T>` belong to `nom::jcfg`, with convenience aliases in
`nom::jcfg::types`.
Mapping and application validation are separate. Config discovery, file loading,
runtime substitution, and command-line overrides remain to be implemented.

Generate one config from the complete example:

```bash
python3 tools/jcfggen.py \
    --source docs/json_config/examples/complete_config/md_capture_jcfg.json \
    --config prod --output-dir build/dev/genconfig
```

This writes `build/dev/genconfig/prod/md_capture.json`. Use `--list` instead of
`--output-dir` to inspect section metadata without evaluating imports or variables.
Use `--input-dir DIR` instead of `--source FILE` to discover `*_jcfg.json` files recursively.
See [config generation](docs/json_config/config_generation.md) for the full CLI.

From the repository root, the engine can be used independently:

```python
from tools.jcfg import generate_configs

configs = generate_configs(
    "docs/json_config/examples/complete_config/md_capture_jcfg.json",
    sections=["prod"],
)
print(configs["prod"].gen_file)  # md_capture.json
print(configs["prod"].config)    # resolved application JSON as a Python dict
```

`generate_configs()` returns a dictionary keyed by section name, preserving source
order, and writes no files.
Omit `sections` to resolve all sections with generation enabled. Generation
variables use the process environment unless an explicit `environment` mapping
is supplied. Invalid configuration raises `config_error` with source context.

`generate_configs_with_inputs()` accepts the same arguments and returns
`(configs, input_paths)`. The paths are a set of absolute `Path` objects with
symbolic links resolved, covering the source and donor files read for selected
sections and their ancestors. The CLI uses them to protect inputs from output writes.

Run the engine and CLI tests directly, or through the normal CTest workflow:

```bash
python3 -m unittest discover -s tests/unit/jcfg -p '*_test.py' -v
python3 -m unittest discover -s tests/integration/jcfggen -p '*_test.py' -v
```

## Build

Install the tools listed in [docs/kit/build.md](docs/kit/build.md#toolchain), then:

```bash
./bootstrap.sh                   # fresh builds, tests and checks for every preset but release
cmake --workflow --preset dev    # configure, build and test one preset
```

Everyday commands, the presets and what bootstrap checks: [docs/kit/build.md](docs/kit/build.md).

## Docs

Agents start at [AGENTS.md](AGENTS.md), people at [docs/](docs/README.md). Adding a library, an
app, tests or a dependency: [docs/kit/howto.md](docs/kit/howto.md). Releases: [CHANGELOG.md](CHANGELOG.md).
