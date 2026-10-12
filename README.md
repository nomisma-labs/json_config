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
application JSON in memory, with tests covering the documented examples and
language rules. It uses Python 3.10+ and the standard library; no package
installation is needed.

The `jcfggen.py` command line and output publication, C++ library, and
generation-specific CMake helpers remain to be implemented. The existing
`cmake_template` config collector and pinned nlohmann/json dependency are available.

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

Run the engine tests directly, or through the normal CTest workflow:

```bash
python3 -m unittest discover -s tests/unit/jcfg -p '*_test.py' -v
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
