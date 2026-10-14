# Library consumer tests

This standalone CMake project consumes json_config in two ways: `find_package`
against an installation and `FetchContent` against the source tree. The repository
registers both through `nom_library_usage_tests()`; package tests also require
`nom_install`.

From the repository root, after building a preset with tests and installation enabled:

```sh
ctest --test-dir build/ci -R 'json_config_library_usage_' --output-on-failure
```

The consumer's model imports only `nom.config_types`. Its executable adds
`NOM_JCONFIG` bindings and checks mapping, serialization, defaults, diagnostics,
hard-wired loading, and command-line parsing. Separate compile-failure tests reject
unsupported config member types.

A CTest fixture runs `jcfggen.py` on [consumer_jcfg.json](consumer_jcfg.json).
The consumer loads the generated files through `parse_config_arguments()` and
`load_config()`: the valid section checks inheritance, child overrides, and C++
defaults; the invalid section must report the expected validation stage, file, and
message. Selecting either loading test also runs its generation fixture.

The generator comes from this repository as test tooling. In package mode the C++
consumer uses the installed library, headers, and module interfaces; these tests do
not require installation of the generator or generation-specific CMake helpers.
