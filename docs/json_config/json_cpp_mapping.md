# JSON/C++ mapping

Mapping connects JSON values to application-owned C++ config structures. The
[loader](app_config_binding.md#loading-and-overrides) handles config resolution,
reading, environment substitution, and runtime overrides. The mapping layer knows
the C++ types; the loader and generator do not need to know individual config members.

## Declaring a config

Import `nom.json_config` for JSON mapping and the common config types in `nom`.
It re-exports `nom.config_types`; data-only code can import that module directly.
The `nom::cfg_types` namespace provides aliases for a convenient scoped using-directive.
Include `<json_config/config.h>` for `NOM_JCONFIG` and the nlohmann/json dependency.

The Nomisma macro `NOM_JCONFIG` declares which members participate in JSON/C++
mapping and checks that their types are enabled for config use. By default, JSON
keys have the same names as the C++ members:

```cpp
#include <json_config/config.h>

import std;

import nom.json_config;

namespace demo
{
    using namespace nom::cfg_types;

    struct connection_config
    {
        cfg_string address;
        cfg_defaulted<cfg_int> timeout_s{5};
        cfg_optional<cfg_string> interface_name;

        std::expected<void, std::string> validate() const
        {
            if (*timeout_s <= 0)
                return std::unexpected("timeout_s must be positive");
            return {};
        }
    };

    NOM_JCONFIG(connection_config, address, timeout_s, interface_name)

    connection_config make_connection(const nlohmann::json& document)
    {
        auto candidate = document.get<connection_config>();
        if (const auto result = candidate.validate(); !result)
            throw std::runtime_error(result.error());
        return candidate;
    }
}
```

For `{"address": "localhost"}`, this returns a config with a timeout of `5` and
an empty optional. `nlohmann::json serialized = config;` converts it back to JSON.
Place `NOM_JCONFIG` in the same namespace as the config type.

The runnable [config_workflow_example](../../apps/config_workflow_example/README.md) combines
`NOM_JCONFIG`, defaulted and optional members, and root validation with the loader.

Nested config structures declare their own mappings. Supported config types
include booleans, strings, checked numeric types, nested configs, vectors, and
string-keyed maps. Plain numeric members such as `int` and `double` are rejected
at compile time; use `cfg_int`, `cfg_uint`, or `cfg_double` from `nom::cfg_types`.

Serialization and deserialization use nlohmann/json directly. Custom domain
types can be explicitly enabled and supply normal `from_json`/`to_json` converters.
The [type guide](https://github.com/nomisma-labs/base/blob/master/docs/base/config_types.md) describes the format-independent
types, aliases, and CMake target. Linking `nom::json_config` supplies that target
transitively, including when consuming an installed package.

Use `document.get<Config>()` to create a config, or `document.get_to(candidate)`
to populate an existing candidate whose initial values should be preserved.
Conversion failures propagate the converter's exceptions. Validation is a
separate call.

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

## Enum mapping

Use `NOM_JCONFIG_ENUM` to register an enum as a config type and define its JSON
string representation. Include `<json_config/config.h>`, import `std` and
`nom.json_config`, and declare the mapping in the enum's namespace, before using
it in a structure mapping:

```cpp
namespace demo
{
    enum class log_level
    {
        debug,
        info,
        warn
    };

    NOM_JCONFIG_ENUM(log_level,
                     {{log_level::debug, "debug"},
                      {log_level::info, "info"},
                      {log_level::warn, "warn"}})

    struct log_config
    {
        nom::cfg_defaulted<log_level> level{log_level::info};
    };

    NOM_JCONFIG(log_config, level)
}
```

Here, `{"level":"debug"}` selects `log_level::debug`; `{}` and `{"level":null}`
preserve the initialized `info` value. Declaring a plain `log_level level` instead
makes the field required, even if it has a C++ initializer. An optional enum uses
the same absent/null rules as other optional members.

The mapping is a nonempty list of `{enumerator, string}` pairs. Matching is exact
and case-sensitive. Additional names for an enum value can be listed explicitly;
the first matching entry is used, so the first name for a value is its canonical
serialized form. For example, adding `{log_level::info, "INFO"}` after the `"info"`
entry accepts both spellings and serializes either as `"info"`.

Unknown strings and non-string JSON values throw `nlohmann::json::type_error`.
Serialization also throws for any enum value without a mapping. There is no
fallback to the first entry. Deserialization errors preserve the JSON path in
nested objects and containers, and failed conversion leaves the destination enum
unchanged. `cfg_defaulted` and optional wrappers handle `null` before invoking
the enum converter; a required enum rejects it.

## Validation boundaries

Mapping checks object structure and required keys, then uses each type's
nlohmann/json converter. Numeric adapters validate the source before conversion:
for example, the JSON adapter for `cfg_int` rejects fractional numbers rather than
truncating them. Application validation checks domain-specific values, ranges,
and relationships between fields.

Config structures support `std::expected<void, std::string> validate() const`.
Return `{}` on success or `std::unexpected(message)` on failure. The root config's
validation includes checks of its nested configs and propagates any error.

After mapping the final runtime values, call the root config's `validate()` before
using the config to initialize application services. Make the resulting config
available to the application only after mapping and validation succeed. Mapping
errors come from nlohmann/json or custom converters; validation errors carry the
supplied message. Mapping into an existing candidate may change some members
before a failure, so keep that candidate separate from the active config.

Component settings can remain independent of JSON and of this library. Application
adapters supply their mapping, so logger, transport, and strategy components can be
used with another configuration system.

The implementation details are in
[Implementation details using nlohmann/json](nlohmann_implementation.md).
