# Config types and serialization dependencies

This repository owns the format-independent `nom.config_types` module, separately
from JSON mapping, checked conversions, and presence handling. Moving the common
module to a future `nom_core` repository is deferred; no such dependency exists now.

## Why special config types were introduced

nlohmann/json allows numeric conversions that can lose information: mapping
`1.5` to an ordinary C++ `int` produces `1`. Application `validate()` runs after
mapping and cannot detect that the fractional part was discarded. Its
[conditional number conversion guidance](https://json.nlohmann.me/features/types/number_handling/#number-conversion)
recommends checking the stored type and destination range before conversion.

`nom::cfg_int`, `cfg_uint`, and `cfg_double` give us types whose JSON
converters can perform these checks and then delegate to nlohmann. This also works
inside native containers, without a separate serializer or container traversal.
`NOM_JCONFIG` restricts mapped members to config-enabled types so ordinary numeric
members cannot accidentally bypass the checks. The supported types and their
exact conversion rules are listed in the
[implementation notes](nlohmann_implementation.md#config-enabled-types).

These checks apply at the serialization boundary. The types still expose mutable
C++ values; application validation handles domain rules. Separately,
`cfg_defaulted<T>` and `cfg_optional<T>` express presence policies.

## Common types and format adapters

The [common module](config_types.md) defines numeric wrappers, standard
string, boolean and container aliases, presence wrappers, and config-type
eligibility. Types live in `nom`, with convenience aliases in `nom::cfg_types`.
The module depends only on the standard library; applications can construct and
validate their config data without a serialization library.

`nom.json_config` re-exports the common module and supplies the JSON adapter.
Source and installed consumers obtain `nom::config_types` transitively through
`nom::json_config`. Numeric source checks, JSON exceptions, and `null` handling
remain in the adapter; the common wrappers do not validate assignments.

The binding itself remains format-specific: `<json_config/config.h>` includes
nlohmann, and `NOM_JCONFIG` generates functions accepting `nlohmann::json`.
It also enables the structure and checks its mapped members against the common
type rules. Custom types can opt in through `nom::enable_config_type<T>` and
provide their own converters.

## Keeping application data independent

Define data structures using `import nom.config_types;` and attach JSON mappings
in a separate binding file if those structures need to remain independent of
JSON. Using `NOM_JCONFIG` beside a structure is convenient, but that file then
depends on the JSON library.

A future TOML adapter can reuse the same common types with its own conversion
rules. This does not promise that every stored value has an equivalent
representation in both formats; for example, the full unsigned 64-bit range
exceeds the range of a TOML adapter using signed 64-bit integers.

The common module supplies no field reflection or serialization framework.
Sharing member declarations between format mappings remains a separate decision.
