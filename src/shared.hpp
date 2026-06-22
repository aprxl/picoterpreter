#ifndef SHARED_HH
#define SHARED_HH



#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>

static constexpr auto VERSION = "1.000";

/// Type aliases and conversion operators for numeric types.
#define REGISTER_NUMERIC_ALIAS(type, alias) \
  using alias = type; \
  constexpr auto operator""_##alias(const unsigned long long value) -> alias { return static_cast<alias>(value); }

REGISTER_NUMERIC_ALIAS(int8_t, i8)
REGISTER_NUMERIC_ALIAS(int16_t, i16)
REGISTER_NUMERIC_ALIAS(int32_t, i32)
REGISTER_NUMERIC_ALIAS(int64_t, i64)
REGISTER_NUMERIC_ALIAS(uint8_t, u8)
REGISTER_NUMERIC_ALIAS(uint16_t, u16)
REGISTER_NUMERIC_ALIAS(uint32_t, u32)
REGISTER_NUMERIC_ALIAS(uint64_t, u64)
REGISTER_NUMERIC_ALIAS(size_t, usize)
REGISTER_NUMERIC_ALIAS(float, f32)
REGISTER_NUMERIC_ALIAS(double, f64)

#undef REGISTER_NUMERIC_ALIAS

/// A simple compile-time FNV-1a hash function
constexpr auto FNV1AHash(std::string_view str) -> usize {
    usize hash = 14695981039346656037_usize;
    for (const char c : str) {
        hash ^= static_cast<usize>(c);
        hash *= 1099511628211_usize;
    }
    return hash;
}

/// Inlined to-do message. Allows for quick searching that will not accidentally
/// match against another project's to-dos.
#define PICO_TODO(message) (void(0))

#endif
