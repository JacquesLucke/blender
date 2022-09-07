/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A `blender::ChunkedList` is a dynamically growing ordered container for values of type T.
 * It is *not* guaranteed that all values will be stored in one contiguous array. Instead, multiple
 * arrays may be used.
 *
 * Comparison to `blender::Vector`:
 * - `ChunkedList` has better performance when appending many elements, because it does not have to
 *   move existing values.
 * - This also means that `ChunkedList` can be used with types that cannot be moved.
 * - A `ChunkedList` can not be indexed efficiently. So while the container is ordered, one can not
 *   efficiently get the value at a specific index.
 * - Iterating over a `ChunkedList` is a little bit slower, because it may have to iterate over
 *   multiple arrays. That is likely negligible in most cases.
 *
 * `ChunkedList` should be used instead of `Vector` when the following two statements are true:
 * - The elements do not have to be in a contiguous array.
 * - The elements do not have to be accessed with an index.
 */

#include "BLI_allocator.hh"
#include "BLI_math_base.hh"
#include "BLI_memory_utils.hh"
#include "BLI_vector.hh"

namespace blender {

namespace detail {
template<typename T> struct alignas(std::max<size_t>(alignof(T), 8)) ChunkListAllocInfo {
  Vector<MutableSpan<T>> spans;
};
}  // namespace detail

template<typename T,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
         typename Allocator = GuardedAllocator>
class ChunkedList {
 private:
  using AllocInfo = detail::ChunkListAllocInfo<T>;

  T *end_;
  T *capacity_end_;
  AllocInfo *alloc_info_ = nullptr;
  BLI_NO_UNIQUE_ADDRESS TypedBuffer<T, InlineBufferCapacity> inline_buffer_;
  BLI_NO_UNIQUE_ADDRESS Allocator allocator_;

 public:
  ChunkedList(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    end_ = inline_buffer_;
    capacity_end_ = end_ + InlineBufferCapacity;
  }

  ChunkedList(NoExceptConstructor, Allocator allocator = {}) noexcept : ChunkedList(allocator)
  {
  }

  ChunkedList(const ChunkedList &other)
  {
    this->extend(other);
  }

  template<int64_t OtherInlineBufferCapacity>
  ChunkedList(const ChunkedList<T, OtherInlineBufferCapacity, Allocator> &other)
      : ChunkedList(other.allocator())
  {
    this->extend(other);
  }

  const Allocator &allocator() const
  {
    return allocator_;
  }

  template<typename Fn> void foreach_span(Fn &&fn) const
  {
    const T *inline_begin = inline_buffer_;
    if (alloc_info_ == nullptr) {
      if (end_ > inline_begin) {
        fn(Span<T>(inline_begin, end_ - inline_begin));
      }
    }
    else {
      if (InlineBufferCapacity > 0) {
        fn(Span<T>(inline_begin, InlineBufferCapacity));
      }
      for (const Span<T> span : alloc_info_->spans) {
        fn(span);
      }
    }
  }

  template<typename Fn> void foreach_span(Fn &&fn)
  {
    const_cast<const ChunkedList *>(this)->foreach_span(
        [&](const Span<T> span) { fn(MutableSpan(const_cast<T *>(span.data()), span.size())); });
  }

  std::optional<Span<T>> get_span(const int64_t index) const
  {
    /* TODO: Define the indices of individual spans. */
    return {};
  }

  void append(const T &value)
  {
    this->append_as(value);
  }
  void append(T &&value)
  {
    this->append_as(std::move(value));
  }
  template<typename... Args> void append_as(Args &&...args)
  {
    this->ensure_space_for_one();
    BLI_assert(end_ < capacity_end_);
    new (end_) T(std::forward<Args>(args)...);
    end_++;
  }

  template<int64_t OtherInlineBufferCapacity>
  void extend(const ChunkedList<T, OtherInlineBufferCapacity, Allocator> &list)
  {
    list.foreach_span([&](const Span<T> span) { this->extend(span); });
  }

  void extend(const Span<T> values)
  {
    const T *src_data = values.data();
    const int64_t remaining_capacity = capacity_end_ - end_;
    const int64_t copy_to_current_chunk = std::min(values.size(), remaining_capacity);
    const int64_t copy_to_next_chunk = values.size() - copy_to_current_chunk;

    uninitialized_copy_n(src_data, copy_to_current_chunk, end_);
    end_ += copy_to_current_chunk;
    if (copy_to_next_chunk == 0) {
      return;
    }
    this->add_chunk(copy_to_next_chunk);
    try {
      uninitialized_copy_n(src_data + copy_to_current_chunk, copy_to_next_chunk, end_);
    }
    catch (...) {
      /* TODO:
       * - Destruct data in previous chunk.
       * - Free newly allocated chunk.
       * - Reset `end_`. */
      throw;
    }
    end_ += copy_to_next_chunk;
  }

  class Iterator {
   private:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using pointer = const T *;
    using reference = const T &;

    const T *begin_;
    const T *current_;
    const T *end_;
    int64_t span_index_;
    const ChunkedList *chunked_list_;

    friend ChunkedList;

   public:
    constexpr Iterator &operator++()
    {
      current_++;
      if (current_ == end_) {
        span_index_++;
        if (const std::optional<Span<T>> span = chunked_list_->get_span(span_index_)) {
          begin_ = span.data();
          current_ = begin_;
          end_ = begin_ + span.size();
        }
      }
    }

    constexpr friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.chunked_list_ == b.chunked_list_);
      return a.current_ == b.current_;
    }

    constexpr const T &operator*() const
    {
      return *current_;
    }
  };

 private:
  void ensure_space_for_one()
  {
    if (UNLIKELY(end_ >= capacity_end_)) {
      this->add_chunk(1);
    }
  }

  BLI_NOINLINE void add_chunk(const int64_t min_chunk_size)
  {
    int64_t new_chunk_size = min_chunk_size;
    T *new_chunk_begin;
    if (alloc_info_ == nullptr) {
      math::max_inplace(new_chunk_size, std::max<int64_t>(InlineBufferCapacity * 2, 8));
      const size_t allocation_size = sizeof(AllocInfo) +
                                     sizeof(T) * static_cast<size_t>(new_chunk_size);
      void *buffer = allocator_.allocate(allocation_size, alignof(AllocInfo), __func__);
      alloc_info_ = new (buffer) AllocInfo();
      new_chunk_begin = static_cast<T *>(POINTER_OFFSET(buffer, sizeof(AllocInfo)));
    }
    else {
      math::max_inplace(new_chunk_size,
                        std::min<int64_t>(alloc_info_->spans.last().size() * 2, 4096));
      const size_t allocation_size = sizeof(T) * static_cast<size_t>(new_chunk_size);
      new_chunk_begin = static_cast<T *>(
          allocator_.allocate(allocation_size, alignof(T), __func__));
    }
    MutableSpan<T> new_chunk{new_chunk_begin, new_chunk_size};
    alloc_info_->spans.append(new_chunk);
    end_ = new_chunk.data();
    capacity_end_ = new_chunk.data() + new_chunk_size;
  }
};

}  // namespace blender
