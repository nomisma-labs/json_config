// SPDX-FileCopyrightText: 2026 Alexey Tkachenko
// SPDX-License-Identifier: MIT

#pragma once

// GCC 16 diagnoses nlohmann's tagged-union parent tracking as out-of-bounds
// with JSON_DIAGNOSTICS and optimized NDEBUG builds. Limit the workaround to
// the dependency's definitions; application diagnostics remain enabled.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 16 && defined(NDEBUG)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#include <nlohmann/json.hpp>

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 16 && defined(NDEBUG)
#pragma GCC diagnostic pop
#endif
