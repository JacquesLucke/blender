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

namespace chunk_list_detail {

template<typename T> struct RawChunk {
  T *begin;
  T *end_if_inactive;
  T *capacity_end;

  RawChunk(T *begin, T *end_if_inactive, T *capacity_end)
      : begin(begin), end_if_inactive(end_if_inactive), capacity_end(capacity_end)
  {
  }
};

template<typename T> struct alignas(std::max<size_t>(alignof(T), 8)) AllocInfo {
  int64_t active_chunk;
  Vector<RawChunk<T>> raw_chunks;
};

}  // namespace chunk_list_detail

template<typename T,
         int64_t InlineBufferCapacity = default_inline_buffer_capacity(sizeof(T)),
         typename Allocator = GuardedAllocator>
class ChunkList {
 private:
  using RawChunk = chunk_list_detail::RawChunk<T>;
  using AllocInfo = chunk_list_detail::AllocInfo<T>;

  T *active_begin_;
  T *active_end_;
  T *active_capacity_end_;
  AllocInfo *alloc_info_ = nullptr;
  BLI_NO_UNIQUE_ADDRESS TypedBuffer<T, InlineBufferCapacity> inline_buffer_;
  BLI_NO_UNIQUE_ADDRESS Allocator allocator_;

 public:
  ChunkList(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    active_begin_ = inline_buffer_;
    active_end_ = active_begin_;
    active_capacity_end_ = active_end_ + InlineBufferCapacity;
  }

  ChunkList(NoExceptConstructor, Allocator allocator = {}) noexcept : ChunkList(allocator)
  {
  }

  ChunkList(const ChunkList &other) : ChunkList(other.allocator())
  {
    this->extend(other);
  }

  template<int64_t OtherInlineBufferCapacity>
  ChunkList(const ChunkList<T, OtherInlineBufferCapacity, Allocator> &other)
      : ChunkList(other.allocator())
  {
    this->extend(other);
  }

  ChunkList(ChunkList &&other) : ChunkList(other.allocator())
  {
    this->extend(other);
    other.clear();
  }

  ~ChunkList()
  {
    if (alloc_info_ == nullptr) {
      destruct_n<T>(inline_buffer_, active_end_ - inline_buffer_);
    }
    else {
      for (const int64_t i : alloc_info_->raw_chunks.index_range()) {
        RawChunk &raw_chunk = alloc_info_->raw_chunks[i];
        T *begin = raw_chunk.begin;
        T *end = i == alloc_info_->active_chunk ? active_end_ : raw_chunk.end_if_inactive;
        destruct_n(begin, end - begin);
        if (i >= 2) {
          allocator_.deallocate(begin);
        }
      }
      alloc_info_->~AllocInfo();
      allocator_.deallocate(alloc_info_);
    }
  }

  ChunkList &operator=(const ChunkList &other)
  {
    return copy_assign_container(*this, other);
  }

  ChunkList &operator=(ChunkList &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  void clear()
  {
    this->~ChunkList();
    new (this) ChunkList();
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

  bool is_empty() const
  {
    return active_end_ == inline_buffer_;
  }

  int64_t size() const
  {
    int64_t chunk_size_sum = 0;
    this->foreach_chunk([&](const Span<T> chunk) { chunk_size_sum += chunk.size(); });
    return chunk_size_sum;
  }

  int64_t get_chunk_num() const
  {
    if (alloc_info_ == nullptr) {
      return 1;
    }
    return alloc_info_->active_chunk + 1;
  }

  BLI_NOINLINE Span<T> get_chunk(const int64_t index) const
  {
    BLI_assert(index >= 0);
    if (alloc_info_ == nullptr) {
      BLI_assert(index == 0);
      return {inline_buffer_, active_end_ - inline_buffer_};
    }
    BLI_assert(index <= alloc_info_->active_chunk);
    const RawChunk &chunk = alloc_info_->raw_chunks[index];
    const T *begin = chunk.begin;
    const T *end = index == alloc_info_->active_chunk ? active_end_ : chunk.end_if_inactive;
    return {begin, end - begin};
  }

  MutableSpan<T> get_chunk(const int64_t index)
  {
    const Span<T> chunk = const_cast<const ChunkList *>(this)->get_chunk(index);
    return {const_cast<T *>(chunk.data()), chunk.size()};
  }

  T &last()
  {
    BLI_assert(!this->is_empty());
    return *(active_end_ - 1);
  }
  const T &last() const
  {
    BLI_assert(!this->is_empty());
    return *(active_end_ - 1);
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
    BLI_assert(active_end_ < active_capacity_end_);
    new (active_end_) T(std::forward<Args>(args)...);
    active_end_++;
  }

  template<int64_t OtherInlineBufferCapacity>
  void extend(const ChunkList<T, OtherInlineBufferCapacity, Allocator> &list)
  {
    list.foreach_chunk([&](const Span<T> chunk) { this->extend(chunk); });
  }

  void extend(const Span<T> values)
  {
    /* TODO: Exception handling. */
    const T *src_begin = values.data();
    const T *src_end = src_begin + values.size();
    const T *src = src_begin;
    while (src < src_end) {
      const int64_t remaining_copies = src_end - src;
      const int64_t remaining_capacity = active_capacity_end_ - active_end_;
      const int64_t copy_num = std::min(remaining_copies, remaining_capacity);
      uninitialized_copy_n(src, copy_num, active_end_);
      active_end_ += copy_num;

      if (copy_num == remaining_copies) {
        break;
      }
      src += copy_num;
      this->activate_next_chunk();
    }
  }

  T pop_last()
  {
    BLI_assert(!this->is_empty());
    T value = std::move(*(active_end_ - 1));
    active_end_--;
    std::destroy_at(active_end_);

    if (active_end_ > active_begin_) {
      return value;
    }
    if (alloc_info_ == nullptr) {
      return value;
    }
    if (alloc_info_->active_chunk == 0) {
      return value;
    }
    RawChunk &old_chunk = alloc_info_->raw_chunks[alloc_info_->active_chunk];
    old_chunk.end_if_inactive = active_end_;
    int new_active = alloc_info_->active_chunk - 1;
    while (new_active >= 0) {
      RawChunk &chunk = alloc_info_->raw_chunks[new_active];
      if (chunk.begin < chunk.end_if_inactive) {
        break;
      }
      new_active--;
    }
    RawChunk &new_chunk = alloc_info_->raw_chunks[new_active];
    alloc_info_->active_chunk = new_active;
    active_begin_ = new_chunk.begin;
    active_end_ = new_chunk.end_if_inactive;
    active_capacity_end_ = new_chunk.capacity_end;
    return value;
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
    BLI_assert(span_num >= 1);
    const Span<T> span = this->get_chunk(0);
    Iterator it;
    it.chunk_list_ = this;
    it.chunk_index_ = 0;
    it.chunk_num_ = span_num;
    it.begin_ = span.data();
    it.end_ = it.begin_ + span.size();
    it.current_ = it.begin_;
    return it;
  }

  Iterator end() const
  {
    const int64_t span_num = this->get_chunk_num();
    BLI_assert(span_num >= 1);
    const Span<T> last_span = this->get_chunk(span_num - 1);

    Iterator it;
    it.chunk_list_ = this;
    it.chunk_index_ = span_num;
    it.chunk_num_ = span_num;
    it.begin_ = last_span.data();
    it.end_ = it.begin_ + last_span.size();
    it.current_ = it.end_;
    return it;
  }

 private:
  void ensure_space_for_one()
  {
    if (active_end_ >= active_capacity_end_) {
      this->activate_next_chunk();
    }
  }

  void activate_next_chunk()
  {
    if (alloc_info_ != nullptr) {
      RawChunk &old_active_chunk = alloc_info_->raw_chunks[alloc_info_->active_chunk];
      old_active_chunk.end_if_inactive = active_end_;
      BLI_assert(old_active_chunk.capacity_end == active_capacity_end_);

      alloc_info_->active_chunk++;
      if (alloc_info_->active_chunk == alloc_info_->raw_chunks.size()) {
        this->add_chunk(1);
      }
    }
    else {
      this->add_initial_alloc_chunk(1);
      alloc_info_->active_chunk = 1;
    }
    RawChunk &new_active_chunk = alloc_info_->raw_chunks[alloc_info_->active_chunk];
    active_begin_ = new_active_chunk.begin;
    active_end_ = new_active_chunk.end_if_inactive;
    active_capacity_end_ = new_active_chunk.capacity_end;
  }

  BLI_NOINLINE void add_initial_alloc_chunk(const int64_t min_chunk_size)
  {
    const int64_t new_chunk_size = std::max<int64_t>(
        std::max<int64_t>(min_chunk_size, InlineBufferCapacity * 2), 8);
    const size_t allocation_size = sizeof(AllocInfo) +
                                   sizeof(T) * static_cast<size_t>(new_chunk_size);
    void *buffer = allocator_.allocate(allocation_size, alignof(AllocInfo), __func__);
    alloc_info_ = new (buffer) AllocInfo();
    alloc_info_->raw_chunks.append_as(inline_buffer_, active_end_, active_capacity_end_);
    T *new_chunk_begin = static_cast<T *>(POINTER_OFFSET(buffer, sizeof(AllocInfo)));
    T *new_chunk_capacity_end = new_chunk_begin + new_chunk_size;
    alloc_info_->raw_chunks.append_as(new_chunk_begin, new_chunk_begin, new_chunk_capacity_end);
  }

  BLI_NOINLINE void add_chunk(const int64_t min_chunk_size)
  {
    const RawChunk &last_chunk = alloc_info_->raw_chunks.last();
    const int64_t last_chunk_size = last_chunk.capacity_end - last_chunk.begin;
    const int64_t new_chunk_size = std::max<int64_t>(min_chunk_size,
                                                     std::min<int64_t>(last_chunk_size * 2, 4096));
    T *new_chunk_begin = static_cast<T *>(allocator_.allocate(
        sizeof(T) * static_cast<size_t>(new_chunk_size), alignof(T), __func__));
    T *new_chunk_capacity_end = new_chunk_begin + new_chunk_size;
    alloc_info_->raw_chunks.append_as(new_chunk_begin, new_chunk_begin, new_chunk_capacity_end);
  }
};

}  // namespace blender
