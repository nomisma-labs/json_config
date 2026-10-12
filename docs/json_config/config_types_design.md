# Config types and serialization dependencies

This note explains the current design and a possible future separation.
The separation is deferred; the implementation described in
[JSON/C++ mapping](json_cpp_mapping.md) remains in place.

## Why special config types were introduced

nlohmann/json allows numeric conversions that can lose information: mapping
`1.5` to an ordinary C++ `int` produces `1`. Application `validate()` runs after
mapping and cannot detect that the fractional part was discarded. Its
[conditional number conversion guidance](https://json.nlohmann.me/features/types/number_handling/#number-conversion)
recommends checking the stored type and destination range before conversion.

`jint`, `juint`, and `jdouble` give us types whose normal `from_json`
converters can perform these checks and then delegate to nlohmann. This also works
inside native containers, without a separate serializer or container traversal.
`NOM_CONFIG` restricts mapped members to config-enabled types so ordinary numeric
members cannot accidentally bypass the checks. The supported types and their
exact conversion rules are listed in the
[implementation notes](nlohmann_implementation.md#config-enabled-types).

These checks apply at the serialization boundary. The types still expose mutable
C++ values; application validation handles domain rules. Separately,
`defaulted<T>` and optional members express presence policies.

## Why C++ config now depends on JSON

The numeric and default types belong to `nom::jcfg`, with convenience aliases in
`nom::jcfg::types`. They are exported by `nom.json_config` together with their JSON
converters. Although the converters take a template parameter named `Json`, they
call JSON-specific operations and create JSON exceptions. Application structures
using these types therefore depend on the JSON config library.

There is also a binding dependency: `<json_config/config.h>` includes nlohmann,
and `NOM_CONFIG` generates functions accepting `nlohmann::json`. It combines
config-type enablement with JSON binding. Using it in the same file as a config
structure introduces JSON support into that file.

This coupling comes from where the types and converters are defined. The numeric
and presence policies themselves can be independent of a serialization format.

## Possible separation

A future change could put value types, presence policies, and config-type
enablement in a small format-neutral module. JSON converters, JSON-specific
checks, exceptions, and `null` handling would remain in a separate JSON adapter
that uses nlohmann directly.

Applications would define their config structures using the neutral types and
attach JSON mappings in a separate binding file. A future TOML adapter could
reuse the same structures with its own supported representations. The common
types would have no dependency on either adapter.

This also requires separating the responsibilities currently combined by
`NOM_CONFIG`; changing names alone would not remove the dependency. Sharing one
member declaration between formats would require neutral field metadata and is
a separate design decision. No generic serialization framework is required for
the proposed separation.
