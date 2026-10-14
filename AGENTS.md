# AGENTS.md

<!-- repo-description:begin -->
This is **json_config**, Nomisma's application configuration library based on JSON.
Its scope is Python configuration generation, C++ configuration discovery and
JSON/C++ mapping, and build-system integration. The Python resolution engine,
generator CLI, generation-specific CMake helpers, C++ mapping module, locator,
runtime loader, and their tests are implemented.
The public C++ API is `nom.json_config`, with `NOM_JCONFIG` in
`<json_config/config.h>`. This repo owns the format-independent `nom::cfg_*`
types in `nom.config_types` and their JSON mapping and numeric conversion checks.
The config collector and nlohmann/json dependency are inherited from `cmake_template`.
Boost (program_options, filesystem and iostreams) is also inherited and found by
`nom_find_boost()`.
<!-- repo-description:end -->

Read and follow the rules every Nomisma repo shares: @docs/kit/agents.md

Rules for this repo alone follow below.

- Example applications must have their own `README.md` with build, usage, and test
  instructions. Keep example-specific details there and use short links from the
  repository README.
