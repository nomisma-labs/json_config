# JSON application config library

**jcfg** is JSON based app config language described in [jcfg.md](jcfg.md)

JSON app config framework has 4 components supporting various config related workflows:
1. [Python config generator: jcfggen.py](config_generation.md);
1. [Build system integration](build_system_integration.md) : config collector and generator integration
1. [C++ config loader and app config resolution](app_config_binding.md);
1. [JSON/C++ mapping](json_cpp_mapping.md);

[Implementation details using nlohmann/json](nlohmann_implementation.md).

Runnable examples have their own build, usage, and test instructions:
[config_example](../../apps/config_example/README.md) covers loading, mapping,
and generated sections; [config_args_example](../../apps/config_args_example/README.md)
adds application arguments through Boost.Program_options.

[Config types and serialization dependencies](config_types_design.md) explains
the separation between common C++ config data and JSON mapping. The
[type guide](config_types.md) lists the types and their CMake integration.

Implementation availability is tracked in the [repository status](../../README.md#implementation-status).
