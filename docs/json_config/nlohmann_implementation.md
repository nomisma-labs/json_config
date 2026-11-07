# Implementation details using nlohmann/json

This document describes the use of nlohmann/json for the
[Nomisma mapping contract](json_cpp_mapping.md). The repository includes
nlohmann/json 3.12.0.

`<json_config/nlohmann_json.h>` includes the native library and is also included
by `config.h`. It scopes a GCC 16 release-build `-Warray-bounds` workaround to
nlohmann's diagnostic parent tracking; application warnings remain enabled.

The [config types design note](https://github.com/nomisma-labs/base/blob/master/docs/base/config_types_design.md) explains why the special
types were introduced and how common C++ data is separated from JSON mapping.
The `base` dependency's `nom.config_types` module owns the types; `nom.json_config`
supplies their JSON converters and re-exports the common module.

## Overview of nlohmann/json facilities

nlohmann/json uses `from_json` and `to_json` converters declared in the same
namespace as the C++ type. Its macros can generate those converters for ordinary
structures. This example uses our checked integer type with a native nlohmann
macro:

```cpp
#include <json_config/nlohmann_json.h>

import std;

import nom.json_config;

namespace demo
{
    using namespace nom::cfg_types;

    struct connection_config
    {
        cfg_string address;
        cfg_int timeout_s;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(connection_config, address, timeout_s)
}
```

Given a parsed JSON object named `document`, the corresponding operations are:

```cpp
auto config = document.get<demo::connection_config>();
nlohmann::json serialized = config;
```

Use `config.timeout_s` directly in numeric expressions or when passing it to a
function accepting its underlying integer type. The native macro invokes
`cfg_int`'s JSON converter, so a floating-point JSON value for `timeout_s`
still fails conversion.

| nlohmann/json facility | Behavior relevant to config mapping |
|---|---|
| `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` | Reads every listed key with `at()`; missing keys fail. |
| `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT` | Uses a newly constructed default object for omitted keys across the whole structure. It does not implement per-member presence policies. |
| `get<T>()` | Creates a new value and invokes its converter. |
| `get_to(existing)` | Passes an existing value to its converter. Preserving its defaults still depends on that converter. |

Use `NOM_JCONFIG` for application config structures. It enables the structure as
a config type, enforces config-enabled member types, adds the per-member presence
policies below, and delegates value conversion to nlohmann/json.

## Config-enabled types

Only enabled types may appear in a `NOM_JCONFIG` mapping. The check applies
recursively to wrappers and container elements.

| Type | Supported representation |
|---|---|
| `nom::cfg_bool` (`bool`) | JSON boolean |
| `nom::cfg_string` (`std::string`) | JSON string |
| `nom::cfg_int` | JSON integer in the `std::int64_t` range |
| `nom::cfg_uint` | Nonnegative JSON integer in the `std::uint64_t` range |
| `nom::cfg_double` | JSON number that converts to a finite `double` |
| A structure declared with `NOM_JCONFIG` | JSON object; the macro enables the structure automatically |
| An enum declared with `NOM_JCONFIG_ENUM` | Explicitly mapped JSON string; unmapped values fail conversion |
| `nom::cfg_vector<T>` (`std::vector<T>`) | JSON array of enabled values |
| `nom::cfg_map<T>` or `nom::cfg_unordered_map<T>` | JSON object with enabled mapped values |

All common types have aliases in `nom::cfg_types`; both spellings name the same
types. Standard-library aliases remain usable under their original names. The
[type guide](https://github.com/nomisma-labs/base/blob/master/docs/base/config_types.md) lists their definitions.

`cfg_defaulted<T>` and optional members apply the presence policies described
below. Plain numeric types such as `int`, `unsigned`, and `double` are not enabled.
The numeric config types are aggregates with a public `value` member and mutable
and const `get()` accessors. They implicitly convert by value to their underlying
type (`std::int64_t`, `std::uint64_t`, or `double`); the conversion is `constexpr`
and `noexcept`. JSON conversion continues to use the checked converters.

## Mapping and custom converters

Include `<json_config/config.h>` and import `nom.json_config`, as shown in the
[usage example](json_cpp_mapping.md#declaring-a-config). Link the CMake target
`nom::json_config`. Its public dependency on `nom::config_types` supplies the
common module. The installed `json_config` package finds the `base` package, which
exports the common target; JSON consumers still need only `find_package(json_config)`.

| Operation | Purpose |
|---|---|
| `document.get<Config>()` | Create and populate a config using its converter. |
| `document.get_to(candidate)` | Populate an existing candidate. |
| `nlohmann::json document = config` | Serialize a config using its converter. |

Custom converters belong in the same namespace as their C++ type, following the
normal nlohmann/json conventions. For example:

```cpp
void from_json(const nlohmann::json& document, domain_type& value);
void to_json(nlohmann::json& document, const domain_type& value);
```

Use `NOM_JCONFIG` for ordinary structure mappings. For renamed keys or domain
types represented by a scalar or collection, write a converter using `at()`,
`get_to()`, assignment, and other nlohmann/json facilities directly.

For enums represented by strings, use
[`NOM_JCONFIG_ENUM`](json_cpp_mapping.md#enum-mapping). It supplies both converters
and the config-type registration. It uses exact matching and rejects unmapped
values in both directions, rather than falling back to another enumerator.

To use a custom type as a config member, explicitly enable it before its first
use in `NOM_JCONFIG`:

```cpp
template <>
inline constexpr bool nom::enable_config_type<domain_type> = true;
```

Enabling a type does not supply its converters. Those converters remain
responsible for checking the type's accepted JSON representations, including any
required numeric or enum constraints.

Conversion failures propagate nlohmann/json or custom-converter exceptions.
`get_to()` may modify the destination before failing. Map a private candidate,
validate it, and publish it only when both operations succeed.

The `nom::json_config` target enables nlohmann's `JSON_DIAGNOSTICS=1` for the
library and its consumers, including installed-package consumers. Native
exceptions then identify the failing JSON value by its JSON Pointer. Our numeric
converters also include the rejected value, for example:

```text
[json.exception.type_error.302] (/i1/ui) expected an integer, got 2.0
```

This [nlohmann option](https://json.nlohmann.me/api/macros/json_diagnostics/)
adds one parent pointer per JSON value and some runtime overhead. Code exchanging
JSON objects with config consumers must use the same diagnostic setting.

## Default and optional wrappers

Use an ordinary member for a required value, a default wrapper for an omittable
value with a C++ default, and `cfg_optional<T>` (an alias of `std::optional<T>`)
for a value that can be absent.
The default wrapper is `nom::cfg_defaulted<T>`, an aggregate with a public
`value` member and mutable and const `get()` accessors. With
`using namespace nom::cfg_types;`, initialize it as `cfg_defaulted<cfg_int> timeout_s{5}`. The
[usage example](json_cpp_mapping.md#declaring-a-config) combines all three policies.

`cfg_defaulted<T>` implicitly converts to `T&` or `const T&` and provides mutable and
const `operator->` and `operator*`. These operations are `constexpr` and `noexcept`
and access the stored value without copying. Use `config.connection->address` for
a defaulted object and `*timeout_s > 0` for a defaulted number. Dereferencing the
number avoids needing two implicit user-defined conversions, which C++ does not
chain. A defaulted value is always present, so no presence check is needed.

| Member | Missing key | Explicit `null` | Other supplied value |
|---|---|---|---|
| Ordinary `T` | Error, even if the member has a C++ initializer | Accepted only if the mapping for `T` supports it | Map to `T` |
| `cfg_defaulted<T>` | Retain the initialized value | Retain the initialized value | Map to the wrapped `T` |
| `cfg_optional<T>` | Retain the initialized optional | Clear it to `std::nullopt` | Map to the contained `T` |

An optional may itself have an initial value. Omitting its key preserves that
value; explicit `null` clears it. A malformed supplied value fails mapping and
does not trigger a default fallback.

The default wrapper serializes as its underlying value, with no extra JSON object.
An engaged optional serializes as its value; an empty optional serializes as `null`.
Serialization may therefore make defaults and absent values explicit even when
their keys were omitted from the input.

The small optional-member helper handles presence and delegates the contained
value to `get_to()`. The pinned nlohmann/json version already serializes optionals,
but does not deserialize them in the current configuration. This helper supports
`std::optional<T>` members with an enabled contained type. Optional container
elements and `cfg_defaulted<std::optional<T>>` are not supported. There is no
global specialization of `nlohmann::adl_serializer<std::optional<T>>`.

## Object shape and nested defaults

Every structure mapped by `NOM_JCONFIG` requires a JSON object. Numbers, strings,
arrays, and `null` fail at that boundary. An enclosing default or optional wrapper
handles its own `null` rule before invoking the structure's mapping.

Initialize a private C++ config candidate once, then populate its members. For
nested objects, map into the already initialized member so omitted defaulted and
optional fields retain values supplied by the parent.

For example, a connection type may declare a timeout of `5`, while its parent
initializes that connection with a timeout of `42`. Supplying only the connection's
`address` must preserve `42`. Reconstructing a fresh connection during mapping
would incorrectly restore `5`. Required fields such as `address` remain required
when that connection object is supplied.

Passing an existing value to `get_to()` preserves its defaults only if the converter
preserves the destination's initialized members. The stock `WITH_DEFAULT` converter
constructs a new fallback object and cannot provide this behavior. For an engaged
optional containing a structure, populate the existing contained value. For an
empty optional, construct the contained value before mapping it.

Enabled containers use nlohmann/json's own converters and representations.
Their elements use the converters for their respective types.

Do not fill missing required JSON keys by serializing defaults
and merging them into the input: that would hide required-key errors.

Publish the candidate only after complete mapping and root `validate()` succeed.
A failed conversion must not expose a partially updated config to the application.

## Value conversion

Direct nlohmann/json conversion to ordinary numeric types can lose information:
`1.5` converted to `int` produces `1`. Root `validate()` cannot recover the original
fractional value afterward, so config numeric types check the source first.

The JSON converters for `cfg_int` and `cfg_uint` reject booleans, floating-point
values (including `1.0`), and integers outside their destination range.
The `cfg_uint` converter also rejects negative integers. The `cfg_double`
converter accepts integer and floating-point JSON numbers, requires a finite
result, and allows normal floating-point rounding. It also rejects nonfinite
values during serialization.

These checks live in this library's JSON converters, not in the common types.
nlohmann/json invokes the same converters inside wrappers and containers; there
is no separate conversion framework. Application-specific converters can add
domain constraints. Keep source-value checks separate from the root
[validate() protocol](json_cpp_mapping.md#validation-boundaries).
