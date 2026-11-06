# json_config tools

This directory contains the repository's configuration tools. Shared build and
maintenance tools live in [tools/kit](../kit/README.md).

| File | Purpose | Documentation |
|---|---|---|
| [config_path.sh](config_path.sh) | Sourceable Bash commands for selecting config search directories. | [Bash setup and commands](../../docs/json_config/app_config_binding.md#selecting-search-directories-in-bash) |
| [jcfggen.py](jcfggen.py) | Command-line JSON config generator and metadata listing. | [Config generation](../../docs/json_config/config_generation.md) |
| [jcfg.py](jcfg.py) | Python engine that resolves jcfg sections into application JSON in memory. | [Python API](../../docs/json_config/config_generation.md#python-api) |
| [jcfg_build.py](jcfg_build.py) | Utility called by CMake to manage generation plans, output ownership, and successful build records. | [Build integration](../../docs/json_config/build_system_integration.md) |

Run the generator from the repository root:

```bash
python3 tools/json_config/jcfggen.py --help
```
