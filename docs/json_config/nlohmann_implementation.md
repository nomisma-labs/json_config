# Implementation details using nlohmann/json

This document describes the use of nlohmann/json for the
[Nomisma mapping contract](json_cpp_mapping.md). The repository includes
nlohmann/json 3.12.0.

## Overview of nlohmann/json facilities

nlohmann/json uses `from_json` and `to_json` converters declared in the same
namespace as the C++ type. Its macros can generate those converters for ordinary
structures:

```cpp
#include <nlohmann/json.hpp>

#include <string>

namespace demo
{
    struct connection_config
    {
        std::string address;
        int timeout_s;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(connection_config, address, timeout_s)
}
```

Given a parsed JSON object named `document`, the corresponding operations are:

```cpp
auto config = document.get<demo::connection_config>();
nlohmann::json serialized = config;
```

| nlohmann/json facility | Behavior relevant to config mapping |
|---|---|
| `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` | Reads every listed key with `at()`; missing keys fail. |
| `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT` | Uses a newly constructed default object for omitted keys across the whole structure. It does not implement per-member presence policies. |
| `get<T>()` | Creates a new value and invokes its converter. |
| `get_to(existing)` | Passes an existing value to its converter. Preserving its defaults still depends on that converter. |

`NOM_CONFIG` uses its own member reader and writer to apply the presence and
conversion rules below. Neither stock macro provides those rules.
Custom converters must follow the same rules when reading their own members.

## Default and optional wrappers

Use an ordinary member for a required value, a default wrapper for an omittable
value with a C++ default, and `std::optional<T>` for a value that can be absent.
The default wrapper is `nom::jcfg::default_value<T>`:

```cpp
struct connection_config
{
    std::string address;
    nom::jcfg::default_value<int> timeout_s{5};
    std::optional<std::string> interface_name;

    std::expected<void, std::string> validate() const;
};

NOM_CONFIG(connection_config, address, timeout_s, interface_name)
```

In this example, `address` is required, omitted `timeout_s` keeps `5`, and omitted
`interface_name` remains empty.

| Member | Missing key | Explicit `null` | Other supplied value |
|---|---|---|---|
| Ordinary `T` | Error, even if the member has a C++ initializer | Accepted only if the mapping for `T` supports it | Map to `T` |
| `default_value<T>` | Retain the initialized value | Retain the initialized value | Map to the wrapped `T` |
| `std::optional<T>` | Retain the initialized optional | Clear it to `std::nullopt` | Map to the contained `T` |

An optional may itself have an initial value. Omitting its key preserves that
value; explicit `null` clears it. A malformed supplied value fails mapping and
does not trigger a default fallback.

The default wrapper serializes as its underlying value, with no extra JSON object.
An engaged optional serializes as its value; an empty optional serializes as `null`.
Serialization may therefore make defaults and absent values explicit even when
their keys were omitted from the input.

The pinned dependency's current settings require explicit optional conversion.
Keep this handling in the Nomisma member reader and writer, including recursive
conversion of the contained value. This avoids a global specialization of
`nlohmann::adl_serializer<std::optional<T>>` that could conflict with another adapter.

## Object shape and nested defaults

Every mapped structure requires a JSON object. Reject numbers, strings, arrays,
and `null` at that boundary. An enclosing default or optional wrapper handles its
own `null` rule before invoking the structure's mapping.

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

A supplied container replaces its contents; each element goes through the same
checked mapping. Do not fill missing required JSON keys by serializing defaults
and merging them into the input: that would hide required-key errors.

Publish the candidate only after complete mapping and root `validate()` succeed.
A failed conversion must not expose a partially updated config to the application.

## Checked value conversion

The stock numeric converters can silently truncate or wrap values. For example,
converting `1.5` to an integer can produce `1`, and converting `-1` to `uint32_t`
can produce `4294967295`. A later `validate()` cannot recover the original value.

The Nomisma converter checks the source value before conversion:

| C++ destination | Acceptance rule |
|---|---|
| Integer | JSON integer within the destination's range; reject floating-point values, including `1.0`, and booleans. Unsigned destinations also reject negative values. |
| Floating-point | JSON number that converts to a finite, in-range result; ordinary floating-point rounding is allowed. |
| Boolean | JSON boolean only. |
| String | JSON string only; runtime text substitution is handled by the loader. |
| Enum | Explicitly recognized representation; reject unknown values. |

Apply these checks through nested configs, wrappers, and container elements.
Delegating a whole container to unchecked conversion would bypass the policy.
Domain-specific representations use explicit adapters. Ordinary path strings are
passed through unchanged unless their adapter defines normalization.

The standard `NLOHMANN_JSON_SERIALIZE_ENUM` macro falls back to its first mapping
for an unknown value. Config enums therefore need a converter that rejects unknown
representations. Keep mapping failures separate from the application's
[validate() protocol](json_cpp_mapping.md#validation-boundaries).
