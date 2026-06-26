#ifndef PICO_BYTE_ARENA_HH
#define PICO_BYTE_ARENA_HH

#include <shared.hpp>
#include <algorithm>
#include <cstring>

namespace pico::detail
{
  class StringBuilder;

  /// A type-erased bump allocator for byte-aligned data, such as strings. Memory is
  /// handed out from a list of fixed chunks that are never moved or freed individually,
  /// so every pointer returned from `allocate` stays valid for the arena's lifetime.
  class ByteArena {
  public:
    explicit ByteArena(const usize chunk_size) : chunk_size_(chunk_size) { }
    ByteArena(const ByteArena&) = delete;
    auto operator=(const ByteArena&) -> ByteArena& = delete;

    ~ByteArena( ) {
      for (const Chunk& chunk : chunks_) {
        operator delete(chunk.data);
      }
    }

    /// Allocates `size` bytes. Existing allocations are never relocated.
    auto Allocate(usize size) -> u8* {
      if (size == 0) {
        size = 1;
      }
      if (chunks_.empty( ) || chunks_.back( ).offset + size > chunks_.back( ).capacity) {
        /// A request larger than a chunk gets its own oversized chunk.
        AddChunk(std::max(chunk_size_, size));
      }
      Chunk& chunk = chunks_.back( );
      u8* const result = chunk.data + chunk.offset;
      chunk.offset += size;
      return result;
    }

    /// Returns a growable builder for creating a string of unknown length. The builder
    /// owns a region that may move as it grows. The final pointer is stable.
    auto Dynamic(usize size) -> StringBuilder;

  private:
    struct Chunk {
      u8* data;
      usize capacity;
      usize offset;
    };

    void AddChunk(const usize capacity) {
      chunks_.push_back(Chunk{
        .data = static_cast<u8*>(operator new(capacity)),
        .capacity = capacity,
        .offset = 0,
      });
    }

    std::vector<Chunk> chunks_;
    usize chunk_size_;
  };

  /// Accumulates characters into an arena, growing by copying the bytes
  /// into a fresh region. Call `finish` once to null-terminate and
  /// obtain the stable pointer.
  class StringBuilder {
  public:
    StringBuilder(ByteArena& arena, char* data, const usize capacity)
      : arena_(arena), data_(data), capacity_(capacity) { }

    auto Append(const char c) -> void {
      if (size_ == capacity_) {
        Grow( );
      }
      data_[size_++] = c;
    }

    [[nodiscard]] auto size( ) const -> usize { return size_; }
    [[nodiscard]] auto view( ) const -> std::string_view { return { data_, size_ }; }

    /// Null-terminates the accumulated bytes and returns a stable pointer into the arena.
    [[nodiscard]] auto Finish( ) -> const char* {
      if (size_ == capacity_) {
        Grow( );
      }
      data_[size_] = '\0';
      return data_;
    }

  private:
    void Grow( ) {
      const usize new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
      const auto new_data = reinterpret_cast<char*>(arena_.Allocate(new_capacity));
      std::memcpy(new_data, data_, size_);
      data_ = new_data;
      capacity_ = new_capacity;
    }

    ByteArena& arena_;
    char* data_;
    usize capacity_;
    usize size_{ };
  };

  inline auto ByteArena::Dynamic(const usize size) -> StringBuilder {
    const usize capacity = (size == 0) ? 1 : size;
    return StringBuilder{ *this, reinterpret_cast<char*>(Allocate(capacity)), capacity };
  }

  class StringPool {
    static constexpr usize CHUNK_SIZE = 64_usize * 1024;
  public:
    StringPool( ) = default;

    /// Copies `string` into the arena with a null terminator and returns a stable pointer.
    auto Intern(const std::string_view& string) -> const char* {
      const auto data = arena_.Allocate(string.size( ) + 1);
      std::memcpy(data, string.data( ), string.size( ));
      data[string.size( )] = '\0';
      return reinterpret_cast<const char*>(data);
    }

    auto Dynamic(const usize size) -> StringBuilder { return arena_.Dynamic(size); }

  private:
    ByteArena arena_{ CHUNK_SIZE };
  };
}

#endif
