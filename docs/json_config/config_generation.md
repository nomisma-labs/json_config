# Config generation

`jcfggen.py` produces application JSON from jcfg sources. It can run directly or
through the [build system](build_system_integration.md), using Python 3.10+ and
the standard library. No Python package installation is needed for this tool.
The [jcfg definition](jcfg.md) owns the language rules.

## Inputs and outputs

The tool accepts either one source file or an input directory tree, such as the
collected config directory:

```text
config/<app>/<app>_jcfg.json          source definition
config/<app>/*.json                  app-owned donor files
genconfig/<section>/<gen_file>       generated application config
```

Discovery scans recursively for `*_jcfg.json`, including files directly in the
input directory. The app name comes from the filename without `_jcfg.json`;
parent directory names do not affect discovery.

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
With `--input-dir`, the input and output directory trees must not overlap. With
`--source`, the output may be a build subdirectory beside the source, but neither
the source nor any imported donor may reside inside the output tree. These checks
also follow symbolic links. A destination that is already a directory, or conflicts
with another output's file/directory path, is an error.

## Command line

| Argument | Meaning |
|---|---|
| `--source FILE` | Read one jcfg file directly. |
| `--input-dir DIR` | Recursively discover `*_jcfg.json` files under `DIR`. Mutually exclusive with `--source`. |
| `--app NAME` | Select exactly one `NAME_jcfg.json` anywhere under `--input-dir`; otherwise process all discovered sources. |
| `--config NAME` | Select a section; repeat to select several. Requires `--source` or `--app`. Omission selects all generating sections. |
| `--output-dir DIR` | Required output root when generating files. |
| `--list` | Report app, section, resolved output filename, and `generate` flag as JSON without generating files. No output directory is required. |
| `--verbose` | Include source and output paths in diagnostics. |

Exactly one input mode is required. An unknown app or section, an ambiguous
`--app` match, or discovery that finds no jcfg files is an error. App selection
happens before reading source contents, so unrelated apps are not evaluated.
A source whose sections are all suppressed is valid and produces no files when
no explicit section selection is requested.

`--list` prints a JSON array with `app`, `section`, `gen_file`, and `generate`
fields. Discovered sources are sorted by app name, then path; sections retain
source order. In `--source` mode, the app name is the source basename without `_jcfg.json` (or the
filename stem for another suffix). Listing a suppressed section explicitly is
allowed. Verbose messages and errors go to stderr, keeping stdout usable as JSON.

For one application:

```bash
python3 tools/json_config/jcfggen.py --input-dir build/dev/config --app md_capture \
    --config prod --output-dir build/dev/genconfig
```

To generate directly from the [complete example](../../apps/md_capture/config/md_capture_jcfg.json):

```bash
python3 tools/json_config/jcfggen.py \
    --source apps/md_capture/config/md_capture_jcfg.json \
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

Each file replacement is atomic; the complete output set is not a filesystem
transaction. A publication failure returns nonzero and removes temporary files,
but completed replacements may remain. The [build integration](build_system_integration.md#build-sequence)
owns success records and removal of stale outputs; the standalone CLI leaves
unselected or previously generated files in place.

`--list` reads metadata and resolves output names without evaluating application
values or environment placeholders. Without a section filter it reports all
sections, including suppressed ones. It supports build planning and deployment
selection; it is not application validation. The [C++ loader](app_config_binding.md)
performs runtime substitution, overrides, mapping, and validation.
