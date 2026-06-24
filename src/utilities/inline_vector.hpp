#ifndef PICO_INLINE_VEC_HH
#define PICO_INLINE_VEC_HH

#include "shared.hpp"
#include <cassert>

namespace pico::detail
{
  /// A constexpr vector implementation with known capacity. Used in the parser to define a constexpr map for
  /// instructions. Provides O(n) look-up method which is used throughout the parser, but that's ok since
  /// `n<=3` in most scenarios.
  template <typename T, size_t N>
  class InlineVector {
  public:
    constexpr InlineVector() = default;
    constexpr InlineVector(std::initializer_list<T> elements) {
      assert(elements.size( ) <= N && "Inline vector initializer list exceeded the supported capacity.");
      for (const T& element : elements) {
        array_[size_++] = element;
      }
    }

    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return size_; }
    [[nodiscard]] constexpr auto begin() const noexcept -> const T* { return array_.data( ); }
    [[nodiscard]] constexpr auto end() const noexcept -> const T* { return array_.data( ) + size_; }

    [[nodiscard]] constexpr auto operator[](size_t index) -> T& { return array_[index]; }
    [[nodiscard]] constexpr auto operator[](size_t index) const -> const T& { return array_[index]; }

    [[nodiscard]] constexpr auto contains(const T& element) const noexcept -> bool {
      for (const T& el : *this) {
        if (el == element) {
          return true;
        }
      }
      return false;
    }

  private:
    std::array<T, N> array_{ };
    usize size_{ };
  };
}

#endif