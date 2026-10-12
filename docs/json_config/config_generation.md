# Config generation

`jcfggen.py` produces application JSON from jcfg sources. It can run directly or
through the [build system](build_system_integration.md), using Python 3.10+ and
the standard library. No Python package installation is needed for this tool.
The [jcfg definition](jcfg.md) owns the language rules.

## Inputs and outputs

The tool accepts either one source file or a collected config directory:

```text
config/<app>/<app>_jcfg.json          source definition
config/<app>/*.json                  app-owned donor files
genconfig/<section>/<gen_file>       generated application config
```

The output filename follows `jcfg.gen_file`, its inheritance rules, and the
source-derived default. Section names are exact identifiers; they need not be
deployment environments. The generator does not invent a `base` section.

Only sections with `jcfg.generate: true` produce files; omission means `true`.
Suppressed sections can still supply inherited values and filenames. Selecting
a suppressed section explicitly for generation is an error. Selecting a child
resolves its ancestors without automatically writing their outputs.

Two results claiming the same output path are an error. Output section names and
filenames must stay within the output directory; absolute output paths and path
traversal are rejected. Input and output locations must be separate.

## Command line

| Argument | Meaning |
|---|---|
| `--source FILE` | Read one jcfg file directly. |
| `--input-dir DIR` | Discover `<app>/<app>_jcfg.json` beneath a collected config directory. Mutually exclusive with `--source`. |
| `--app NAME` | Select one app under `--input-dir`; otherwise process all discovered apps. |
| `--config NAME` | Select a section; repeat to select several. Requires `--source` or `--app`. Omission selects all generating sections. |
| `--output-dir DIR` | Required output root when generating files. |
| `--list` | Report app, section, resolved output filename, and `generate` flag as JSON without generating files. No output directory is required. |
| `--verbose` | Include source and output paths in diagnostics. |

Exactly one input mode is required. An unknown app or section, or discovery that
finds no jcfg files, is an error. A source whose sections are all suppressed is
valid and produces no files when no explicit section selection is requested.

For one application:

```bash
python3 tools/jcfggen.py --input-dir build/dev/config --app md_capture \
    --config prod --output-dir build/dev/genconfig
```

To generate directly from the [complete example](examples/complete_config/md_capture_jcfg.json):

```bash
python3 tools/jcfggen.py \
    --source docs/json_config/examples/complete_config/md_capture_jcfg.json \
    --config prod --output-dir build/dev/genconfig
```

Both examples select `prod/md_capture.json`. Omitting `--config` in the second
command also generates the other enabled sections, including
`prod-liquidation/md_capture_liquidation.json`. To process all collected apps,
omit both `--app` and `--config` from the first command.

## Resolution and errors

The generator checks metadata and inheritance for every section, then resolves
imports and instructions only for selected sections and their ancestors using
the merge rules in [jcfg.md](jcfg.md). Unrelated sections do not require their
donor files or generation variables to be available. `jcfg.name`, `jcfg.extend`,
and `jcfg.gen_file` are literal strings; they do not support placeholders.
Relative imports are resolved from the source file's directory. Imported ordinary
JSON stays literal during generation.
Fragment references use `file.json:/pointer`; see the
[fragment import rules](jcfg.md#import-a-fragment-of-a-json-file).

`{jcfg:gen-var NAME}` is resolved during generation; an unset variable is an error.
`{jcfg:run-var NAME}` remains in the output for substitution when loading the config.
The generator does not require the runtime environment or know the application's C++ types.
Import paths can use generation-time variables such as `{jcfg:gen-var NOM_CONFIG_LIB}`,
provided through the process environment or supplied by the build system. Expand
these placeholders before resolving and reading the files; the tool must not guess
library locations from the caller's working directory.

Missing files or fragments, malformed JSON, invalid metadata, unknown instructions
in jcfg-authored content, and failed delete/override operations stop generation.
Diagnostics identify the source file, section, and property or import involved.
Return zero on success, nonzero on failure; do not substitute empty objects for
failed imports or publish a partially resolved config.

Resolve and check all selected results before publishing them. Use temporary files
and replace completed outputs; leave identical output files unchanged. The build
must record success only after every selected output is published. A failed run
must not make older files appear to be current successful results.

`--list` reads metadata and resolves output names without evaluating application
values or environment placeholders. Without a section filter it reports all
sections, including suppressed ones. It supports build planning and deployment
selection; it is not application validation. The [C++ loader](app_config_binding.md)
performs runtime substitution, overrides, mapping, and validation.
