/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A `blender::ChunkList` is a dynamically growing ordered container for values of type T.
 * It is *not* guaranteed that all values will be stored in one contiguous array. Instead, multiple
 * arrays may be used.
 *
 * Comparison to `blender::Vector`:
 * - `ChunkList` has better performance when appending many elements, because it does not have to
 *   move existing values.
 * - This also means that `ChunkList` can be used with types that cannot be moved.
 * - A `ChunkList` can not be indexed efficiently. So while the container is ordered, one can not
 *   efficiently get the value at a specific index.
 * - Iterating over a `ChunkList` is a little bit slower, because it may have to iterate over
 *   multiple arrays. That is likely negligible in most cases.
 *
 * `ChunkList` should be used instead of `Vector` when the following two statements are true:
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
  Vector<MutableSpan<T>> chunks;
};
}  // namespace detail

template<typename T,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
         typename Allocator = GuardedAllocator>
class ChunkList {
 private:
  using AllocInfo = detail::ChunkListAllocInfo<T>;

  T *end_;
  T *capacity_end_;
  AllocInfo *alloc_info_ = nullptr;
  BLI_NO_UNIQUE_ADDRESS TypedBuffer<T, InlineBufferCapacity> inline_buffer_;
  BLI_NO_UNIQUE_ADDRESS Allocator allocator_;

 public:
  ChunkList(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    end_ = inline_buffer_;
    capacity_end_ = end_ + InlineBufferCapacity;
  }

  ChunkList(NoExceptConstructor, Allocator allocator = {}) noexcept : ChunkList(allocator)
  {
  }

  ChunkList(const ChunkList &other)
  {
    this->extend(other);
  }

  template<int64_t OtherInlineBufferCapacity>
  ChunkList(const ChunkList<T, OtherInlineBufferCapacity, Allocator> &other)
      : ChunkList(other.allocator())
  {
    this->extend(other);
  }

  ~ChunkList()
  {
    if (InlineBufferCapacity > 0) {
      if (alloc_info_ == nullptr) {
        destruct_n<T>(inline_buffer_, end_ - inline_buffer_);
      }
      else {
        for (const int64_t i : IndexRange(this->get_chunk_num()).drop_front(2)) {
          MutableSpan<T> chunk = this->get_chunk(i);
          destruct_n(chunk.data(), chunk.size());
          allocator_.deallocate(chunk.data());
        }
        const MutableSpan<T> inline_chunk = alloc_info_->chunks[0];
        const MutableSpan<T> alloc_chunk = alloc_info_->chunks[1];
        destruct_n(inline_chunk.data(), inline_chunk.size());
        destruct_n(alloc_chunk.data(), alloc_chunk.size());
        alloc_info_->~AllocInfo();
        allocator_.deallocate(alloc_info_);
      }
    }
    else {
      if (alloc_info_ != nullptr) {
        for (const int64_t i : IndexRange(this->get_chunk_num()).drop_front(1)) {
          MutableSpan<T> chunk = this->get_chunk(i);
          destruct_n(chunk.data(), chunk.size());
          allocator_.deallocate(chunk.data());
        }
        const MutableSpan<T> alloc_chunk = alloc_info_->chunks[1];
        destruct_n(alloc_chunk.data(), alloc_chunk.size());
        alloc_info_->~AllocInfo();
        allocator_.deallocate(alloc_info_);
      }
    }
  }

  const Allocator &allocator() const
  {
    return allocator_;
  }

  template<typename Fn> void foreach_chunk(Fn &&fn) const
  {
    for (const int64_t i : IndexRange(this->get_chunk_num())) {
      const Span<T> chunk = this->get_chunk(i);
      fn(chunk);
    }
  }

  template<typename Fn> void foreach_chunk(Fn &&fn)
  {
    const_cast<const ChunkList *>(this)->foreach_chunk([&](const Span<T> chunk) {
      fn(MutableSpan(const_cast<T *>(chunk.data()), chunk.size()));
    });
  }

  template<typename Fn> void foreach_elem(Fn &&fn) const
  {
    for (const int64_t i : IndexRange(this->get_chunk_num())) {
      const Span<T> chunk = this->get_chunk(i);
      for (const T &value : chunk) {
        fn(value);
      }
    }
  }

  int64_t get_chunk_num() const
  {
    if constexpr (InlineBufferCapacity > 0) {
      if (alloc_info_ == nullptr) {
        return 1;
      }
      return alloc_info_->chunks.size();
    }
    else {
      if (alloc_info_ == nullptr) {
        return 0;
      }
      return alloc_info_->chunks.size();
    }
  }

  BLI_NOINLINE Span<T> get_chunk(const int64_t index) const
  {
    BLI_assert(index >= 0);
    if constexpr (InlineBufferCapacity > 0) {
      if (alloc_info_ == nullptr) {
        BLI_assert(index == 0);
        return {inline_buffer_, end_ - inline_buffer_};
      }
      BLI_assert(index < alloc_info_->chunks.size());
      if (index == alloc_info_->chunks.size() - 1) {
        const T *span_begin = alloc_info_->chunks.last().data();
        return {span_begin, end_ - span_begin};
      }
      return alloc_info_->chunks[index];
    }
    else {
      BLI_assert(alloc_info_ != nullptr);
      return alloc_info_->chunks[index];
    }
  }

  MutableSpan<T> get_chunk(const int64_t index)
  {
    const Span<T> chunk = const_cast<const ChunkList *>(this)->get_chunk(index);
    return {const_cast<T *>(chunk.data()), chunk.size()};
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
  void extend(const ChunkList<T, OtherInlineBufferCapacity, Allocator> &list)
  {
    list.foreach_chunk([&](const Span<T> chunk) { this->extend(chunk); });
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
    int64_t chunk_index_;
    int64_t chunk_num_;
    const ChunkList *chunk_list_;

    friend ChunkList;

   public:
    constexpr Iterator &operator++()
    {
      BLI_assert(chunk_index_ < chunk_num_);
      current_++;
      if (current_ == end_) {
        chunk_index_++;
        if (chunk_index_ < chunk_num_) {
          const Span<T> next_chunk = chunk_list_->get_chunk(chunk_index_);
          begin_ = next_chunk.data();
          current_ = begin_;
          end_ = begin_ + next_chunk.size();
        }
      }
      return *this;
    }

    constexpr friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.chunk_list_ == b.chunk_list_);
      return a.current_ != b.current_;
    }

    constexpr const T &operator*() const
    {
      return *current_;
    }
  };

  Iterator begin() const
  {
    const int64_t span_num = this->get_chunk_num();
    Iterator it;
    it.chunk_list_ = this;
    it.chunk_index_ = 0;
    it.chunk_num_ = span_num;
    if (span_num == 0) {
      it.begin_ = nullptr;
      it.end_ = nullptr;
    }
    else {
      const Span<T> span = this->get_chunk(0);
      it.begin_ = span.data();
      it.end_ = it.begin_ + span.size();
    }
    it.current_ = it.begin_;
    return it;
  }

  Iterator end() const
  {
    const int64_t span_num = this->get_chunk_num();
    Iterator it;
    it.chunk_list_ = this;
    it.chunk_index_ = span_num;
    it.chunk_num_ = span_num;
    if (span_num == 0) {
      it.begin_ = nullptr;
      it.end_ = nullptr;
    }
    else {
      const Span<T> last_span = this->get_chunk(span_num - 1);
      it.begin_ = last_span.data();
      it.end_ = it.begin_ + last_span.size();
    }
    it.current_ = it.end_;
    return it;
  }

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
      alloc_info_->chunks.append({inline_buffer_, end_ - inline_buffer_});
      new_chunk_begin = static_cast<T *>(POINTER_OFFSET(buffer, sizeof(AllocInfo)));
    }
    else {
      math::max_inplace(new_chunk_size,
                        std::min<int64_t>(alloc_info_->chunks.last().size() * 2, 4096 * 1000));
      const size_t allocation_size = sizeof(T) * static_cast<size_t>(new_chunk_size);
      new_chunk_begin = static_cast<T *>(
          allocator_.allocate(allocation_size, alignof(T), __func__));
    }
    MutableSpan<T> new_chunk{new_chunk_begin, new_chunk_size};
    alloc_info_->chunks.append(new_chunk);
    end_ = new_chunk.data();
    capacity_end_ = new_chunk.data() + new_chunk_size;
  }
};

}  // namespace blender
