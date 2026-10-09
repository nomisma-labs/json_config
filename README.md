# json_config

<!-- repo-description:begin -->
**json_config** is Nomisma's application configuration library based on JSON.
It shares its design with [toml_config](https://github.com/nomisma-labs/toml_config),
with similar capabilities planned for both format implementations.
<!-- repo-description:end -->

The planned scope includes layered configuration generation with a Python tool
(`jcfggen.py`), C++ config resolution, JSON/C++ mapping, and build-system
integration for collecting and generating app configs.

This repository currently contains the initialized `cmake_template` scaffold and
the template's pinned nlohmann/json dependency. The configuration generator and
C++ library are still to be implemented.

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
