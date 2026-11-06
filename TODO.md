# Implementation status and TODO

Current capabilities and follow-up work for discussion and review. Review the
implementation first, then revisit application initialization. The convenience
API and user-local config layer below are proposals, with design decisions still open.

## Implementation status

The [configuration documentation](docs/json_config/README.md) defines the library's
behavior. The [Python generator engine](tools/json_config/jcfg.py) resolves jcfg sections to
application JSON in memory. The [jcfggen.py CLI](tools/json_config/jcfggen.py) lists metadata
and writes generated configs, preserving unchanged output files. Both use Python
3.10+ and the standard library; no package installation is needed.

The [CMake helpers](docs/json_config/build_system_integration.md) register generation,
config tests and installation, and provide `config` and `clean_config` targets.
App builds collect configs; generation is requested with `config` for the whole
build or `<project>_generate_jcfg_<app>` for one app, or opted into with
`nom_jcfg_generate(... BUILD_WITH_APP)`. Omit `SECTIONS` to generate all sections with
generation enabled. The helpers reuse the existing
`cmake_template` config collector.

The [JSON/C++ mapping library](docs/json_config/json_cpp_mapping.md) is available
through `import nom.json_config;` and the CMake target `nom::json_config`.
`NOM_JCONFIG` in `<json_config/config.h>` maps application structures with required,
defaulted, and optional members using nlohmann/json. It enforces supported config
types, including checked JSON conversions for `cfg_int`, `cfg_uint`, and `cfg_double`.
The [common config types](https://github.com/nomisma-labs/base/blob/master/docs/base/config_types.md) belong to `nom`, with
convenience aliases in `nom::cfg_types`. The `base` dependency owns the format-independent
`nom.config_types` module, re-exported by `nom.json_config`; its `nom::config_types` target is a
transitive dependency for both source and installed consumers. Data-only consumers can use
`find_package(base)` and link `nom::config_types` without the JSON library.
Mapping and application validation are separate. The
[C++ locator and loader](docs/json_config/app_config_binding.md) resolve a config,
read JSON, substitute runtime variables, apply command-line overrides, then map
and validate the root C++ type. Hard-wired defaults are supported when lookup finds
no file. `load_config<Config>()` returns the typed value and selected source/path.
The command-line adapter uses Boost.Program_options through the shared build kit.

## 1. Review key source files and workflows

Review each significant script and C++ source, together with its tests and the
workflows it supports:

- [ ] [tools/json_config/jcfg.py](tools/json_config/jcfg.py)
- [ ] [tools/json_config/jcfggen.py](tools/json_config/jcfggen.py)
- [ ] [tools/json_config/jcfg_build.py](tools/json_config/jcfg_build.py)
- [ ] [core/json_config/config_loader.cpp](core/json_config/config_loader.cpp)
- [ ] [core/json_config/config_locator.cpp](core/json_config/config_locator.cpp)
- [ ] [core/json_config/config_options.cpp](core/json_config/config_options.cpp) — revisit enum stringification.
- [ ] [core/json_config/config_runtime.cppm](core/json_config/config_runtime.cppm) — review whether the name reflects its responsibilities.
- [ ] Work through the build, usage, and test instructions in every example README:
  [config_workflow_example](apps/config_workflow_example/README.md),
  [config_args_example](apps/config_args_example/README.md), and
  [md_capture](apps/md_capture/README.md).

## 2. Revisit application initialization

Do this after the source review. Cover command-line arguments, config discovery,
and loading. The goal is to make the common case concise while allowing
applications to define and validate their own arguments.

### Current behavior and constraints to preserve

- `main()` owns initialization, services, error reporting, and exit codes. Prefer
  meaningful startup code there over a `run_app()` wrapper.
- `parse_config_arguments()` extracts framework options and preserves the remaining
  arguments. In `config_args_example`, the application parses those with
  Boost.Program_options and handles help before calling `load_config<Config>(options)`.
- Loading resolves the config source, reads JSON (or uses hard-wired defaults),
  substitutes runtime variables, applies command-line overrides, maps to C++, and
  validates. The library returns the load result or reports errors to its caller.
- Application help and argument validation happen before config I/O. After
  successful loading, `--check-config` exits before application services start.

See [application integration](docs/json_config/app_config_binding.md#application-integration)
and the working examples:
[config_workflow_example](apps/config_workflow_example/README.md) and
[config_args_example](apps/config_args_example/README.md). Keep detailed usage in
their READMEs and link to it contextually from other docs.

### Proposal and open questions

- [ ] Consider `load_config<Config>(argc, argv, key)` for config-only applications,
  reusing the existing implementation. Keep separate parsing and loading interfaces
  available for more elaborate applications.
- [ ] Decide the overload's return shape and handling of unexpected arguments.
- [ ] Decide how to expose standard config help alongside application help.
- [ ] Decide how much diagnostic reporting belongs in the example.

## 3. Add a user-local config layer

This is a convenience feature to design first. **User-local config layer** is the
working name: it describes the additional source of settings, while "override"
describes its effect. Naming is not settled.

The initial idea is to keep local, possibly unversioned JSON files that the
generator imports and automatically applies to any generated config as its final
JSON merge. Unlike a standalone development config, which is a base config that
does not require generation, this layer would customize generated configs without
editing each one.

- [ ] Agree on the name: **user-local config layer**, **user config layer**, or
  **local config overrides**.
- [ ] Resolve when the layer is applied. The requested precedence is runtime
  `run-var` substitution, then the user-local layer, then command-line overrides.
  Currently, generation preserves `{jcfg:run-var NAME}` for substitution by the
  C++ loader. A final generation-time merge would therefore happen before
  `run-var` substitution. Decide how to reconcile these two parts of the proposal.
- [ ] Define file naming, discovery, and scope: how local files are selected and
  how their automatic application across generated configs is controlled.
- [ ] Define merge behavior, ordering between multiple local files, and handling
  of missing or invalid local files.

## 4. Improve command-line usability

- [ ] Make `--help` print more explanatory help, generated by
  `boost::program_options`, with descriptions of the available options and their usage.
- [ ] Add short option aliases, including `-c` for `--config` and `-k` for
  `--check-config`, and show both forms in the generated help.
- [ ] Add `full`, `view`, and `silent` modes to `--check-config` with the following output:

| Mode | Output |
|---|---|
| `full` | All details about config locations, followed by the final configuration. |
| `view` | Only the final configuration. |
| `silent` | Just `OK`, preserving the current behavior. |

In `full` and `view` modes, print the final normalized configuration after runtime
substitutions and any command-line overrides have been applied.
