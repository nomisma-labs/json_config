# Common configuration types

This repository's module `nom.config_types` defines configuration data types directly in
namespace `nom`. It depends only on the standard library. Applications can construct and
validate these values without a configuration file or a serialization library;
format-specific adapters supply loading, mapping and conversion checks separately.

## Types

| Type | Storage and purpose |
|---|---|
| `cfg_bool` | Alias of `bool`. |
| `cfg_string` | Alias of `std::string`; adapters use UTF-8 for configuration text. |
| `cfg_int` | Numeric wrapper storing `std::int64_t`. |
| `cfg_uint` | Numeric wrapper storing `std::uint64_t`. |
| `cfg_double` | Numeric wrapper storing `double`. |
| `cfg_vector<T>` | Alias of `std::vector<T>`. |
| `cfg_map<T>` | Alias of `std::map<std::string, T>`. |
| `cfg_unordered_map<T>` | Alias of `std::unordered_map<std::string, T>`. |
| `cfg_optional<T>` | Alias of `std::optional<T>` for an optional structure member. |
| `cfg_defaulted<T>` | Aggregate wrapper holding a value used when a member is absent. |

All types are also available in `nom::cfg_types` for convenient unqualified use:

```cpp
using namespace nom::cfg_types;
cfg_optional<cfg_uint> worker_count;
```

Numeric wrappers have a public `value` field, mutable and const `get()` accessors, and conversion
to their underlying numeric type. Aggregate initialization is straightforward:

```cpp
#include <string>

import nom.config_types;

struct connection_config
{
    nom::cfg_string host{"localhost"};
    nom::cfg_uint port{9000};
    nom::cfg_double timeout_s{2.5};
};

connection_config config;
std::string address = config.host;
double timeout = config.timeout_s;
```

`cfg_defaulted<T>` exposes its public `value`, `get()`, dereference (`*`), member access (`->`),
and conversion to mutable or const `T&`. It preserves ordinary C++ initialization:

```cpp
nom::cfg_defaulted<connection_config> connection{};
connection->host = "feed.example";
connection_config& settings = connection;
```

The wrappers are transparent storage types. They do not validate assignments or enforce
invariants. Numeric adapters must reject incompatible source types and out-of-range values
before conversion. Application validation checks domain rules such as valid port numbers.

The integer ranges are the full C++ signed and unsigned 64-bit ranges. The shared JSON/TOML
integer subset is signed 64-bit: a `cfg_uint` value above `INT64_MAX` needs a format-specific
representation or must be rejected by a TOML adapter using signed 64-bit storage. `cfg_double`
has ordinary binary floating-point rounding; it does not promise exact decimal or large-integer
representation. Adapters targeting both formats should check for finite values. The common
module contains no parser, serializer or format-specific numeric checks.

## Supported value types

`nom::is_config_value<T>` recognizes the scalar types above and recursively checks vectors,
string-keyed maps and `cfg_defaulted<T>`. `nom::is_config_member<T>` additionally accepts
`cfg_optional<T>` around a supported value. Optional values are intended for structure members,
not array elements or map values.

These traits recognize types, not the spelling of their aliases: `std::string` and `cfg_string`
are the same type. Bare numeric types such as `int` and `double` are not enabled; use their
configuration wrappers.

An application can opt its own structure into the common type system:

```cpp
template <>
inline constexpr bool nom::enable_config_type<connection_config> = true;

static_assert(nom::is_config_member<nom::cfg_optional<connection_config>>);
static_assert(nom::is_config_value<nom::cfg_vector<connection_config>>);
static_assert(!nom::is_config_value<int>);
```

Opting in declares support; it does not inspect structure members, add mapping functions or
invoke validation. A format adapter can use these traits to check members when defining its
mapping. Ordinary C++ structures need no opt-in until they participate in such a check.

## Build and installation

The repository builds `core/json_config/config_types.cppm` with the ordinary `nom_lib()` helper.
Data-only applications link `nom::config_types` and import `nom.config_types`:

```cmake
nom_app(config_example SOURCES main.cpp DEPS nom::config_types)
```

This target has no nlohmann/json dependency. JSON applications link `nom::json_config`,
which supplies it transitively and re-exports its module.

Both targets and their module sources are installed in the same `json_config` CMake package
when `nom_install` is enabled. Installed consumers wanting only the common types use:

```cmake
find_package(json_config CONFIG REQUIRED)
target_link_libraries(app PRIVATE nom::config_types)
```

Consumers rebuild the module interface with their own compiler, under the normal
[module requirements](../kit/build.md#modules). A library exposing these types links
`nom::config_types` through `PUBLIC_DEPS` and lists `json_config` in its
`nom_install_package(DEPENDENCIES ...)`.
