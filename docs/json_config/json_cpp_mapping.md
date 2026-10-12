# JSON/C++ mapping

Mapping connects JSON values to application-owned C++ config structures. The
[loader](app_config_binding.md#loading-and-overrides) handles config resolution,
reading, environment substitution, and runtime overrides. The mapping layer knows
the C++ types; the loader and generator do not need to know individual config members.

## Declaring a config

The framework's C++ types use the `nom::jcfg` namespace.

The Nomisma macro `NOM_CONFIG` declares which members participate in JSON/C++
mapping. By default, JSON keys have the same names as the C++ members:

```cpp
struct connection_config
{
    std::string address;
    int timeout_s;

    std::expected<void, std::string> validate() const;
};

NOM_CONFIG(connection_config, address, timeout_s)
```

This is a declaration example; the application supplies the validation body.
Nested config structures declare their own mappings. The framework also maps
collections of supported values and converts config structures back to JSON.
Custom adapters handle different JSON key names and domain types such as enums,
durations, or paths.

## Required, defaulted, and optional fields

Each member has its own presence policy:

| Policy | Meaning |
|---|---|
| Required | The JSON key must be present. A C++ member initializer alone does not make the key optional. |
| Defaulted | An omitted key retains the value initialized in C++. |
| Optional | The value can be absent; C++ represents that absence explicitly. |

These policies can be combined in one structure. A supplied value must satisfy its
mapping rules. Nested defaults are preserved when a partial nested object supplies
only some defaulted members; required members still need their JSON keys.

The [nlohmann/json implementation notes](nlohmann_implementation.md) describe the default and
optional wrappers, the treatment of `null`, and conversion details.

## Validation boundaries

Mapping checks JSON structure, required keys, value types, and safe conversion to
C++ values. Application validation checks supported values, ranges, and relationships
between fields.

Config structures support `std::expected<void, std::string> validate() const`.
Return `{}` on success or `std::unexpected(message)` on failure. The root config's
validation includes checks of its nested configs and propagates any error.

After mapping the final runtime values, call the root config's `validate()` before
using the config to initialize application services. Make the resulting config
available to the application only after mapping and validation succeed. Mapping
errors identify the JSON field; validation errors carry the supplied message.

Component settings can remain independent of JSON and of this library. Application
adapters supply their mapping, so logger, transport, and strategy components can be
used with another configuration system.

The implementation details are in
[Implementation details using nlohmann/json](nlohmann_implementation.md).
