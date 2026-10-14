# JSON/C++ mapping

Mapping connects JSON values to application-owned C++ config structures. The
[loader](app_config_binding.md#loading-and-overrides) handles config resolution,
reading, environment substitution, and runtime overrides. The mapping layer knows
the C++ types; the loader and generator do not need to know individual config members.

## Declaring a config

Import `nom.json_config` for the config types in `nom::jcfg`. The
`nom::jcfg::types` namespace provides aliases for a convenient scoped using-directive.
Include `<json_config/config.h>` for `NOM_CONFIG` and the nlohmann/json dependency.

The Nomisma macro `NOM_CONFIG` declares which members participate in JSON/C++
mapping and checks that their types are enabled for config use. By default, JSON
keys have the same names as the C++ members:

```cpp
#include <json_config/config.h>

#include <expected>
#include <optional>
#include <stdexcept>
#include <string>

import nom.json_config;

namespace demo
{
    using namespace nom::jcfg::types;

    struct connection_config
    {
        std::string address;
        defaulted<jint> timeout_s{5};
        std::optional<std::string> interface_name;

        std::expected<void, std::string> validate() const
        {
            if (*timeout_s <= 0)
                return std::unexpected("timeout_s must be positive");
            return {};
        }
    };

    NOM_CONFIG(connection_config, address, timeout_s, interface_name)

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
Place `NOM_CONFIG` in the same namespace as the config type.

The runnable [config_example](../../apps/config_example/README.md) combines
`NOM_CONFIG`, defaulted and optional members, and root validation with the loader.

Nested config structures declare their own mappings. Supported config types
include booleans, strings, checked numeric types, nested configs, vectors, and
string-keyed maps. Plain numeric members such as `int` and `double` are rejected
at compile time; use `jint`, `juint`, or `jdouble` from `nom::jcfg::types`.

Serialization and deserialization use nlohmann/json directly. Custom domain
types can be explicitly enabled and supply normal `from_json`/`to_json` converters.

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

## Validation boundaries

Mapping checks object structure and required keys, then uses each type's
nlohmann/json converter. Checked numeric types validate the source before
conversion: for example, `jint` rejects fractional JSON numbers rather than
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
