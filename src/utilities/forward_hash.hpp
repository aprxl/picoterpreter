#ifndef PICO_FORWARD_HASH_HH
#define PICO_FORWARD_HASH_HH

#include <shared.hpp>

namespace pico::detail
{
  /// A runtime FNV1-a hashing class for strings that takes characters individually. Used in the lexer to compute
  /// hashes as the characters themselves are being read.
  class ForwardHash {
  public:
    ForwardHash() = default;

    [[nodiscard]] auto hash() const noexcept -> usize { return hash_; }

    auto AddCharacter(const char c) -> void {
        hash_ ^= static_cast<usize>(c);
        hash_ *= 1099511628211_usize;
    }

  private:
    usize hash_{ 14695981039346656037_usize };
  };
}

#endif
