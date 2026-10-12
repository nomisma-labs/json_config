# jcfg - a structural configuration language

## Objective
Nomisma **jcfg** language defines application configuration using JSON.
The purpose of this language is to add structural features to JSON configs, i.e. ability to compose a single valid config from several fragments. For those who are familiar with OOP this reminds "inheritance" and "aggregation". So instead of redefining potentially complex multi-component config for different purposes we define "base" config with some default parameters and then re-use it by extending it in "derived" configs (inheritance) or by defining common parameters in "donor" config and including parts or whole "donor" into an "acceptor" config (aggregation). Those are main structural mechanisms. Apart from it there would be "adjustment" instructions that controls how these extension (aka inheritance) or aggregation (aka import or inclusion) happens.
Another way of looking at jcfg - a "stackable" or "layered" configuration with each layer introducing or refining previous layers config properties.

## Basic conventions
**jcfg** files by convention have `*_jcfg.json` file suffix. Any jcfg file is a well formed JSON file so it can be processed with regular JSON tools (e.g. pretty printers etc.) The name of the jcfg file will have its special meaning when we come to integration with Nomisma build system, but in this guide we can use any name (usually just `app_jcfg.json`). Nomisma platform provides a tool to generate well formed JSON configs from jcfg so they can be used by Nomisma apps in different cases.

## Motivating example
Just to make it more illustrative let's consider `md_capture` app that have 2 configs - for dev and prod environments (shown schematically):
```
dev:
    log.level = debug
    connection
        snapshot.address = wss://ws-api.binance.com:443/ws-api/v3
        address = wss://stream-sbe.binance.com:9443/ws
    subscription
        symbols = btcusdt
        channels = trade, depth
    mdLog.file = binance-spot-%Y%m%d-%H.md

prod:
    log
        level = warn
        file = log/%Y%m%d/md_capture.log
    connection
        snapshot.address = wss://ws-api.binance.com:443/ws-api/v3
        address = wss://stream-sbe.binance.com:9443/ws
    subscription
        symbols = btcusdt, ethusdt
        channels = trade, depth
    mdLog.file = md/%Y%m%d/binance-spot-%Y%m%d-%H.md
```
As you see even in this small config there are repeating and unique parameters and values. There are multiple ways how we can abstract them out. But first - why would we want to do it?
Here are the cases:
1. A trader/quant maintains a list of required symbols for MD capture. He wants to focus on this list only and not to get into details of app configuration. For that he creates a file `binance-spot-md-capture-symbols.json` and adds the required symbols there. On the other hand a dev, while testing the MD capture app, wants to focus on specific list of symbols (perhaps only one symbol for simplicity).
2. An infra team monitors the connection parameters of Binance MD end point. They learned that Binance migrated to a new infra with different DNS address. Infra team wants to apply this new address for all components homogenously - for that they maintain a single `binance-spot-connection.json` config which contains actual connectivity parameters, so they don't need to update configs of 20+ apps that connect to Binance market data end point. However one trader learned that one particular subscription channel that he subscribes (e.g. liquidation info) was not migrated properly by Binance, so he wants his app to use old connectivity parameters - so he applies an exception to his app config.

One possible way of defining a coherent config for this app and use cases is to:
1. Create base config which would be also a dev config, e.g. `md_capture.json` with complete dev config;
2. Create `md_capture_jcfg.json` which in `base` case imports `md_capture.json`;
3. Add `ci` section for integration environment that inherits base config and imports `binance-spot-connection.json`. It also updates parameters `log` and `mdLog` to standard values for infra deployment;
4. Add `prod` section for main production deployment and inherit it from `ci`. Update `subscription.symbols` parameter - by including it from `binance-spot-md-capture-symbols.json`;
5. Add `prod-liquidation` section to collect liquidation data, inherit it from `prod` and update `connection` and `subscription` parameter sections.

As you see, by defining configuration this way we allowed several teams (dev, QA, devops/infra, researchers, traders) to collaborate and not to step on each other toes while configuring and using md_capture app.

## Structure of jcfg file

A **jcfg** file is a JSON array containing one or more config sections, or a single
section object. The single-object form is equivalent to an array containing that
one section. Each section is an object with a `jcfg` metadata object and application
properties alongside it.
The `jcfg` property is reserved at the section root and is excluded from generated
application JSON. The array preserves section order.

The `jcfg:` marker is reserved for instructions and placeholders; using it as
literal application text is unsupported.

See [app_jcfg.json](examples/stucture_config/app_jcfg.json) for an example with three
sections: `common`, `ci`, and `prod`. Each section defines an application config,
although writing its output file may be suppressed. A section usually represents
an environment (e.g. ci, prod) or a use case (e.g. sim, perf).

The same folder contains [app.json](examples/stucture_config/app.json), the default
application config, and [symbol.json](examples/stucture_config/symbol.json), the
symbol list imported by `prod`.

The structure follows these rules:

1. **Section identity and order.** Each section has a unique `jcfg.name`, including
   in the single-object form. The first section is the root and has no `jcfg.extend`.
   Its name can be any section name, such as `dev`, `sim`, `common`, or `base`;
   Every other section names exactly one parent through `jcfg.extend`, a string
   referring to an earlier section in the same file. All sections therefore derive
   directly or indirectly from the root; forward references and inheritance cycles
   are not allowed.
2. **Merge precedence.** Start with the parent's resolved application config, or
   an empty object for the root. `jcfg.import` accepts a single filename string or
   an array of filename strings; `"app.json"` is equivalent to `["app.json"]`.
   Merge those files in their listed order, then merge the section's local
   application properties. Omitting `jcfg.import` means no section-level imports.
   Objects merge recursively; arrays and other values replace earlier values.
   Later values take precedence.
3. **Metadata inheritance.** Application data is inherited separately from
   metadata. `jcfg.name`, `jcfg.extend`, `jcfg.import`, and `jcfg.generate` describe
   only the current section and are not inherited. An explicit `jcfg.gen_file`
   selects its output filename; when omitted in a derived section, the filename
   is inherited from its parent. See
   [output filenames](#output-filenames-default-inherited-and-explicit) for the
   default naming rule. `jcfg.name`, `jcfg.extend`, and `jcfg.gen_file` are literal
   strings and do not support placeholders.
4. **Property imports.** A jcfg-authored property value such as
   `"jcfg:import symbol.json"` imports JSON data at that property before merging
   it into the inherited/imported configuration. Imported ordinary JSON remains
   literal data during generation. [Fragment imports](#import-a-fragment-of-a-json-file)
   select a value within a donor file.
5. **Config generation.** `jcfg.generate` is a boolean that defaults to `true` for
   each section. Setting it to `false` suppresses that section's output file, while
   its resolved application data and output filename remain available to children.
   Its imports and instructions are evaluated when it is an ancestor of a selected
   section. Children generate their own files by default, even when their parent
   has `jcfg.generate` set to `false`.

Validate metadata and inheritance for every section. Resolve imports and
instructions only for selected sections and their ancestors; unrelated sections
do not require their donor files or generation variables to be available.

In the example, `common` imports `app.json` and sets `log.level` to `debug`, but
`jcfg.generate` is `false`, so it produces no output file. The `ci` section changes
the level to `warn` and inherits the output filename `app.json`.
The `prod` section inherits from `ci`, selects `prod.json` as its output filename,
and imports the symbol list into `subscription.symbols`. Neither child specifies
`jcfg.generate`, so both generate their output files.

A common, optional layout places `<app>_jcfg.json` and a complete application
config, `<app>.json`, in `<app>/config`. The root section can import that JSON
file in full and use the same output filename. Source files and generated files
live in different locations: the build collects source configs under
`build/<preset>/config/`, and generated configs belong under
`build/<preset>/genconfig/`. When the source JSON already contains the desired
development configuration, the app can use it directly without generation.

## Define basic jcfg mechanisms

Each example folder contains a jcfg source, any JSON files it imports, and the
expected application JSON under `genconfig/`. Examples with several sections group
their expected outputs under `genconfig/<section>/`, so sections can share an output
filename. These directories identify the results being illustrated; the build or
generator invocation selects the actual output location.

The examples introduce the mechanisms in this order:

1. [In-place config with an explicit output name](#in-place-config-with-an-explicit-output-name)
1. [Import a JSON config](#import-a-json-config)
1. [Import a fragment of a JSON file](#import-a-fragment-of-a-json-file)
1. [Output filenames: default, inherited and explicit](#output-filenames-default-inherited-and-explicit)
1. [Extend a section](#extend-a-section)
1. [Merge inherited and local parameters](#merge-inherited-and-local-parameters)
1. [Merge several JSON files into an extended section](#merge-several-json-files-into-an-extended-section)
1. [Delete a key and override a value](#delete-a-key-and-override-a-value)
1. [Override with imported content](#override-with-imported-content)
1. [Illustrate all basic mechanisms in one example](#complete-example)

### In-place config with an explicit output name

The [app_jcfg.json](examples/mech_01_config/app_jcfg.json) example defines a complete
application config directly in a single section. All application values are
written in place alongside the section's `jcfg` metadata.

- `jcfg.name` identifies the root section as `dev`.
- `jcfg.gen_file` names the output file `app.json`.
- `log` and `subscription` contain the application settings.

The source file uses the single-object form. Wrapping it in a one-element array
would have the same meaning. Resolving the section produces one JSON object
containing the application settings, with the `jcfg` metadata removed.
`jcfg.generate` is omitted, so it defaults to `true`. The expected output is shown in
[genconfig/app.json](examples/mech_01_config/genconfig/app.json).

### Import a JSON config

In [app_jcfg.json](examples/mech_02_config/app_jcfg.json), the single `dev` section
has `jcfg.import` set to `"app.json"`. This is equivalent to `["app.json"]`; an
array supports merging several whole JSON objects in order. Relative import paths
are resolved from the jcfg source file's directory, so the input is the adjacent
[app.json](examples/mech_02_config/app.json).

The section has no local application properties. Its expected output,
[genconfig/app.json](examples/mech_02_config/genconfig/app.json), therefore contains
the same application data as the imported file. The source and output share a
filename but live in different locations. The source is also a complete config
that the application can use directly.

### Import a fragment of a JSON file

[app_jcfg.json](examples/mech_03_config/app_jcfg.json) imports two arrays from
[markets.json](examples/mech_03_config/markets.json), a donor containing separate
`spot` and `perps` settings:

| Destination property | Import instruction | Selected value |
|---|---|---|
| `subscription.symbols` | `jcfg:import markets.json:/spot/symbols` | The spot symbol array |
| `subscription.channels` | `jcfg:import markets.json:/spot/channels` | The spot channel array |

In `jcfg:import markets.json:/spot/symbols`, the file is `markets.json` and the
JSON Pointer is `/spot/symbols`. The colon separates the filename from the pointer,
which retains its leading `/`. Without a colon, as in `jcfg:import markets.json`,
the whole document is imported. As in the previous example, the donor file is
located relative to the jcfg source.

After expanding generation placeholders, split the import argument (the text after
`jcfg:import`) at its first `:`. The colon is reserved and cannot appear in donor
file paths; any later colons belong to the JSON Pointer.

The pointer follows [JSON Pointer (RFC 6901)](https://www.rfc-editor.org/rfc/rfc6901).
`/spot/symbols/0` selects the first array element. In property names, `~1`
represents `/` and `~0` represents `~`. A missing member or invalid array index
is an error. The selected JSON value retains its type: an array stays an array,
and a string stays a string.

The selected arrays become the property values before the local section is
merged. The unrelated `perps` settings do not enter the application config.
See [genconfig/app.json](examples/mech_03_config/genconfig/app.json).

Fragment imports are also useful for keeping generation-time values in a separate
JSON map. In the same example, [values.json](examples/mech_03_config/values.json)
holds `log_level` and `timeout_s`. `jcfg:import values.json:/log_level` supplies
`log.level`, while `jcfg:import values.json:/timeout_s` supplies `connection.timeout_s`.
The generator copies these values into the output as a string and a number,
respectively, using the same fragment-import mechanism.

Imported ordinary JSON is not interpreted by the generator. Runtime environment
placeholders are resolved when the application loads the config.

### Output filenames: default, inherited and explicit

[app_jcfg.json](examples/mech_04_config/app_jcfg.json) illustrates three naming
cases. The default filename comes from the source filename: replace the
`_jcfg.json` suffix with `.json`. Thus `app_jcfg.json` defaults to `app.json`.

| Section | Filename rule | Expected output |
|---|---|---|
| `dev` | No `jcfg.gen_file`; use the source-derived default | [dev/app.json](examples/mech_04_config/genconfig/dev/app.json) |
| `ci` | No `jcfg.gen_file`; inherit the parent's filename | [ci/app.json](examples/mech_04_config/genconfig/ci/app.json) |
| `prod` | Explicit `jcfg.gen_file` is `capture.json` | [prod/capture.json](examples/mech_04_config/genconfig/prod/capture.json) |

An explicit filename takes precedence over an inherited filename. Importing a
file supplies application data; its filename does not change the section's output
name. The `ci` and `prod` outputs here have the same application data even though
their filenames differ.

### Extend a section

In [app_jcfg.json](examples/mech_05_config/app_jcfg.json), `ci` extends `base`,
and `prod` extends `ci`. Both derived sections contain only `jcfg` metadata.
Their application settings and output filenames are inherited unchanged.

Compare [base/app.json](examples/mech_05_config/genconfig/base/app.json),
[ci/app.json](examples/mech_05_config/genconfig/ci/app.json), and
[prod/app.json](examples/mech_05_config/genconfig/prod/app.json): all three have the
same contents. Each derived section has exactly one parent, and that parent appears
earlier in the source array.

### Merge inherited and local parameters

[app_jcfg.json](examples/mech_06_config/app_jcfg.json) starts with a `base` config
and changes selected settings in `ci`. Compare
[base/app.json](examples/mech_06_config/genconfig/base/app.json) with
[ci/app.json](examples/mech_06_config/genconfig/ci/app.json):

- `log.level` changes from `debug` to `warn`.
- `log.file.name` changes to `ci.log`, while the inherited `rotationSize` remains.
- `connection.timeout_s` changes to `20`, while the inherited address remains.
- `subscription.symbols` becomes `["ethusdt"]`. The inherited symbol array is
  replaced in full; `subscription.channels` remains unchanged.

Object properties merge recursively. At every other value boundary, the later
value replaces the earlier one. Resolving `ci` leaves the resolved `base` config
unchanged, so other sections can still inherit the original values.

### Merge several JSON files into an extended section

In [app_jcfg.json](examples/mech_07_config/app_jcfg.json), `ci` extends `base`,
then imports [connection.json](examples/mech_07_config/connection.json) and
[deployment.json](examples/mech_07_config/deployment.json), in that order.
Local properties are applied last.

The value of `connection.timeout_s` shows the precedence:

| Step | Value |
|---|---|
| Inherit `base` | `5` |
| Merge `connection.json` | `10` |
| Merge `deployment.json` | `20` |
| Merge local properties | `30` |

The final config keeps the address from `connection.json`, the log filename from
`deployment.json`, and the local log level `error`. Other inherited values remain.
See [base/app.json](examples/mech_07_config/genconfig/base/app.json) and
[ci/app.json](examples/mech_07_config/genconfig/ci/app.json).

This is still single inheritance: `ci` has one parent section. The import list
adds ordinary JSON data sources to that inherited configuration. Consider that
multiple import is a powerful tool that can easily confuse both the writer and
the reader of the config, especially when many people collaborate on it. Typical
use case for this feature is building a config for multiple environments (e.g.
dev, research, ci, prod) where different people are responsible for different
parts of the app config. So if the config has such logical separation, then
multiple import is a useable tool. However if the config is not semantically
structured, then multiple import can create a mess.

### Delete a key and override a value

[app_jcfg.json](examples/mech_08_config/app_jcfg.json) applies two instructions
while merging `ci` into its inherited application config:

- `log.file` has the value `"jcfg:delete"`, removing that property. The inherited
  `log.level` remains.
- `subscription` contains a `jcfg:override` wrapper whose value is the complete
  replacement object. The inherited `snapshot` property disappears because the
  replacement contains only `symbols` and `channels`.

Compare [base/app.json](examples/mech_08_config/genconfig/base/app.json) with
[ci/app.json](examples/mech_08_config/genconfig/ci/app.json).

`jcfg:delete` operates on an object property. Deleting a missing property is an
error. JSON `null` remains an ordinary value and does not mean deletion.

An override wrapper contains exactly one property, `jcfg:override`. Its value
replaces the target in full, without recursive merging with the previous value.
It can be an object, array, scalar, or `null`; if the target property is absent,
the override creates it. Neither the delete instruction nor the override wrapper
is written to the generated config.

### Override with imported content

[app_jcfg.json](examples/mech_09_config/app_jcfg.json) uses an import instruction
as the value inside a `jcfg:override` wrapper:

| Section | Replacement source | Expected output |
|---|---|---|
| `base` | Initial in-place settings | [base/app.json](examples/mech_09_config/genconfig/base/app.json) |
| `ci` | `jcfg:import subscription.json` | [ci/app.json](examples/mech_09_config/genconfig/ci/app.json) |
| `prod` | `jcfg:import markets.json:/perps` | [prod/app.json](examples/mech_09_config/genconfig/prod/app.json) |

The `ci` replacement comes from the complete
[subscription.json](examples/mech_09_config/subscription.json) document. The
`prod` replacement comes from the `perps` object inside
[markets.json](examples/mech_09_config/markets.json).

First resolve the import, including its pointer if present. Then assign the
result as the complete `subscription` value. The inherited `snapshot` property
does not survive either replacement. Imports inside an in-place replacement
object are also resolved before that object is assigned.

### Complete example

[md_capture_jcfg.json](examples/complete_config/md_capture_jcfg.json) brings the
mechanisms together for the [motivating example](#motivating-example).

The input files separate the responsibilities:

- [md_capture.json](examples/complete_config/md_capture.json) contains a complete
  development config with literal values. It can be used without generation.
- [connection.json](examples/complete_config/connection.json) supplies the current
  connection settings shared by deployed variants.
- [deployment.json](examples/complete_config/deployment.json) supplies deployment
  logging settings and the market-data output path.
- [symbols.json](examples/complete_config/symbols.json) supplies separate symbol
  lists for ordinary spot capture and liquidation capture.

| Section | How it is assembled | Expected output |
|---|---|---|
| `base` | Import the complete development config | [base/md_capture.json](examples/complete_config/genconfig/base/md_capture.json) |
| `ci` | Extend `base`; import connection and deployment settings | [ci/md_capture.json](examples/complete_config/genconfig/ci/md_capture.json) |
| `prod` | Extend `ci`; import `/spot` from the symbol lists | [prod/md_capture.json](examples/complete_config/genconfig/prod/md_capture.json) |
| `prod-liquidation` | Extend `prod`; restore the old connection, replace the subscription, and adjust logging | [prod-liquidation/md_capture_liquidation.json](examples/complete_config/genconfig/prod-liquidation/md_capture_liquidation.json) |

For the liquidation exception, `connection` is overridden by importing the
`/connection` fragment of the development config. This restores the old settings
as one object. `subscription` is overridden with the liquidation symbol list and
channel, while `log.file` is deleted and `mdLog.file` receives a separate capture
path. The other variants retain their own resolved settings.


## Advanced jcfg generation topics

1. Suppress config generation with `jcfg.generate: false`; see the
   [structure rules and example](#structure-of-jcfg-file).
   The section remains available for inheritance, but the flag is not inherited.
   This property is useful if you created a "service" config section - which is not
   supposed to have an associated environment or use case, but it's used by other
   config sections as an extension base.
1. Placeholder substitution during generation
    `{jcfg:gen-var NAME}` substitutes a generation-time variable supplied through the
    process environment or by the build system. Placeholders are allowed in
    application string values and import paths. A placeholder can occupy the whole
    string or appear within text; surrounding text is preserved. `{jcfg:run-var NAME}`
    placeholders pass through generation unchanged for resolution when loading the config.
    Example:
    ```
    {
        "log": {"file": "{jcfg:gen-var NFS_PATH}/logs"}
    }
    ```
1. Common config libraries
    Config libraries are shared directories of JSON files that app configs can
    import. They avoid duplicating common settings, such as production connection
    parameters used by both a trading strategy and an MD capture app.

    Import paths in `jcfg.import` and `jcfg:import` can start with a location
    placeholder. For example, `NOM_CONFIG_LIB` can point to a well-known config
    library directory in the project. Set it in the process environment, or let
    the build system supply it in the generator's environment:

    ```json
    {
        "jcfg": {
            "name": "prod",
            "import": "{jcfg:gen-var NOM_CONFIG_LIB}/connection.json"
        },
        "symbols": "jcfg:import {jcfg:gen-var NOM_CONFIG_LIB}/markets.json:/spot/symbols"
    }
    ```

    Resolve location placeholders before reading the referenced files. An unset
    variable or missing file fails generation. Expanded absolute paths are used
    directly; relative paths are resolved from the jcfg source file's directory.

## Runtime value overrides

When loading a JSON config file, the application follows this order:

1. Parse the application JSON.
2. Resolve runtime environment placeholders.
3. Apply command-line overrides.
4. Perform JSON/C++ mapping and application validation.

For [hard-wired config](app_config_binding.md#hard-wired-config), command-line
overrides target the JSON serialized from the initialized C++ config.

Each stage must succeed before the next begins. An unset required environment
variable causes failure before command-line overrides, even if an override would
replace that value.

There could be multiple reasons for runtime value overrides, but they all boil down to the fact that some parameter is required to have different value at the time when app is executed (versus the time when the app config was defined and generated), for instance.
1. We want to restart the app with different log.level to get more detailed diagnostics.
1. We want to try running the trading strategy with different parameters (e.g. risk aversion).
1. Binance MD API endpoint address changed and we need to restart MD capture with different address.
1. Order gateway requires private key to connect to Order API endpoint and we don't want to provide this key during config generation, so we set the private key only when we start a trading strategy.

All such cases can be done by updating the app config and restarting the app (or dynamically resetting the app config). However this might be inconvenient. For this case we have other ways of runtime value overrides.

### Types of overrides

1. Placeholder substitution at runtime.
    Example `app.json`:
    ```
    {
        "log":
        {
            "level": "{jcfg:run-var LOG_LEVEL}",
            "file": "{jcfg:run-var NFS_PATH}/logs"
        }
    }
    ```
    `{jcfg:run-var NAME}` substitutes an environment value within the string when
    loading the config.
    With `NFS_PATH=/srv/data`, the example's `log.file` becomes `/srv/data/logs`.
    An unset variable is an error.
2. Command line config override format
    The app accepts `--set <JSON-pointer>=<value>` to replace an existing JSON value.
    A missing target is an error. These overrides run after placeholder substitution,
    so command-line values take precedence.
