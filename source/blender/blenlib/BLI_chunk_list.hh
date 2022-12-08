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
  /**
   * Pointer to the beginning of the allocation of this chunk. Might not be the same as `begin`,
   * because the might be allocated together with #AllocInfo. Can be null when this chunk is
   * inlined into the #ChunkList.
   */
  void *allocation;

  RawChunk() = default;
  RawChunk(T *begin, T *end_if_inactive, T *capacity_end, void *allocation)
      : begin(begin),
        end_if_inactive(end_if_inactive),
        capacity_end(capacity_end),
        allocation(allocation)
  {
  }
};

template<typename T> struct AllocInfo {
  int64_t active;
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

  template<typename OtherT, int64_t OtherInlineBufferCapacity, typename OtherAllocator>
  friend class ChunkList;

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
    this->extend(std::move(other));
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
        T *end = i == alloc_info_->active ? active_end_ : raw_chunk.end_if_inactive;
        destruct_n(begin, end - begin);
        if (i >= 1) {
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
    return alloc_info_->active + 1;
  }

  BLI_NOINLINE Span<T> get_chunk(const int64_t index) const
  {
    BLI_assert(index >= 0);
    if (alloc_info_ == nullptr) {
      BLI_assert(index == 0);
      return {inline_buffer_, active_end_ - inline_buffer_};
    }
    BLI_assert(index <= alloc_info_->active);
    const RawChunk &chunk = alloc_info_->raw_chunks[index];
    const T *begin = chunk.begin;
    const T *end = index == alloc_info_->active ? active_end_ : chunk.end_if_inactive;
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
    try {
      new (active_end_) T(std::forward<Args>(args)...);
    }
    catch (...) {
      this->move_end_back_to_prev_element();
      throw;
    }
    active_end_++;
  }

  template<int64_t OtherInlineBufferCapacity>
  void extend(const ChunkList<T, OtherInlineBufferCapacity, Allocator> &other)
  {
    other.foreach_chunk([&](const Span<T> chunk) { this->extend(chunk); });
  }

  template<int64_t OtherInlineBufferCapacity>
  void extend(ChunkList<T, OtherInlineBufferCapacity, Allocator> &&other)
  {
    AllocInfo *other_alloc_info = other.alloc_info_;

    auto reset_other_active = [&]() {
      other.active_begin_ = other.inline_buffer_;
      other.active_end_ = other.active_begin_;
      other.active_capacity_end_ = other.active_begin_ + OtherInlineBufferCapacity;
    };

    if (other_alloc_info == nullptr) {
      /* Handle case when the other list is fully inline. */
      this->extend_move({other.active_begin_, other.active_end_ - other.active_begin_});
      reset_other_active();
      return;
    }

    /* Make sure all chunks are up to date. */
    other_alloc_info->raw_chunks[other_alloc_info->active].end_if_inactive = other.active_end_;

    /* Always move first chunk. */
    RawChunk &other_inline_chunk = other_alloc_info->raw_chunks[0];
    this->extend_move(
        {other_inline_chunk.begin, other_inline_chunk.end_if_inactive - other_inline_chunk.begin});

    auto update_self_active = [&]() {
      RawChunk &active_chunk = alloc_info_->raw_chunks[alloc_info_->active];
      active_begin_ = active_chunk.begin;
      active_end_ = active_chunk.end_if_inactive;
      active_capacity_end_ = active_chunk.capacity_end;
    };

    /* Try to steal info block. */
    if (alloc_info_ == nullptr) {
      alloc_info_ = other_alloc_info;
      other.alloc_info_ = nullptr;
      alloc_info_->raw_chunks[0] = {active_begin_, active_end_, active_capacity_end_, nullptr};
      reset_other_active();
      update_self_active();
      return;
    }

    alloc_info_->raw_chunks[alloc_info_->active].end_if_inactive = active_end_;

    alloc_info_->raw_chunks.extend(other_alloc_info->raw_chunks.as_span().drop_front(1));
    alloc_info_->active += other_alloc_info->active;
    other_alloc_info->raw_chunks.resize(1);
    other_alloc_info->raw_chunks[0] = {other.inline_buffer_,
                                       other.inline_buffer_,
                                       other.inline_buffer_ + OtherInlineBufferCapacity,
                                       nullptr};
    other_alloc_info->active = 0;
    reset_other_active();
    update_self_active();
  }

  void extend_move(const MutableSpan<T> values)
  {
    this->extend_impl<true>(values);
  }

  void extend(const Span<T> values)
  {
    this->extend_impl<false>(values);
  }

  template<bool UseMove> void extend_impl(const Span<T> values)
  {
    const T *src_begin = values.data();
    const T *src_end = src_begin + values.size();
    const T *src = src_begin;
    while (src < src_end) {
      const int64_t remaining_copies = src_end - src;
      const int64_t remaining_capacity = active_capacity_end_ - active_end_;
      const int64_t copy_num = std::min(remaining_copies, remaining_capacity);
      try {
        if constexpr (UseMove) {
          uninitialized_move_n(const_cast<T *>(src), copy_num, active_end_);
        }
        else {
          uninitialized_copy_n(src, copy_num, active_end_);
        }
      }
      catch (...) {
        if (alloc_info_ != nullptr) {
          int64_t remaining_destructs = src - src_begin;
          while (remaining_destructs > 0) {
            this->move_end_back_to_prev_element();
            const int64_t chunk_size = active_end_ - active_begin_;
            const int64_t destruct_num = std::min(chunk_size, remaining_destructs);
            destruct_n(active_end_ - destruct_num, destruct_num);
            active_end_ -= destruct_num;
            remaining_destructs -= destruct_num;
          }
        }
        throw;
      }
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
    this->move_end_back_to_prev_element();
    return value;
  }

  void move_end_back_to_prev_element()
  {
    if (active_end_ > active_begin_) {
      return;
    }
    if (alloc_info_ == nullptr) {
      return;
    }
    if (alloc_info_->active == 0) {
      return;
    }
    RawChunk &old_chunk = alloc_info_->raw_chunks[alloc_info_->active];
    old_chunk.end_if_inactive = active_end_;
    int new_active = alloc_info_->active - 1;
    while (new_active >= 0) {
      RawChunk &chunk = alloc_info_->raw_chunks[new_active];
      if (chunk.begin < chunk.end_if_inactive) {
        break;
      }
      new_active--;
    }
    RawChunk &new_chunk = alloc_info_->raw_chunks[new_active];
    alloc_info_->active = new_active;
    active_begin_ = new_chunk.begin;
    active_end_ = new_chunk.end_if_inactive;
    active_capacity_end_ = new_chunk.capacity_end;
  }

  template<bool UseConst> class Iterator {
   private:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using pointer = std::conditional_t<UseConst, const T *, T *>;
    using reference = std::conditional_t<UseConst, const T &, T &>;

    pointer begin_;
    pointer current_;
    pointer end_;
    int64_t chunk_index_;
    int64_t chunk_num_;
    std::conditional_t<UseConst, const ChunkList *, ChunkList *> chunk_list_;

    friend ChunkList;

   public:
    constexpr Iterator &operator++()
    {
      BLI_assert(chunk_index_ < chunk_num_);
      current_++;
      if (current_ == end_) {
        chunk_index_++;
        if (chunk_index_ < chunk_num_) {
          auto next_chunk = chunk_list_->get_chunk(chunk_index_);
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

    constexpr reference operator*() const
    {
      return *current_;
    }
  };

  Iterator<true> begin() const
  {
    return this->begin_impl<true>();
  }

  Iterator<true> end() const
  {
    return this->end_impl<true>();
  }

  Iterator<false> begin()
  {
    return this->begin_impl<false>();
  }

  Iterator<false> end()
  {
    return this->end_impl<false>();
  }

 private:
  template<bool UseConst> Iterator<UseConst> begin_impl() const
  {
    const int64_t span_num = this->get_chunk_num();
    BLI_assert(span_num >= 1);
    const Span<T> span = this->get_chunk(0);
    Iterator<UseConst> it;
    it.chunk_list_ = const_cast<ChunkList *>(this);
    it.chunk_index_ = 0;
    it.chunk_num_ = span_num;
    it.begin_ = const_cast<T *>(span.data());
    it.end_ = it.begin_ + span.size();
    it.current_ = it.begin_;
    return it;
  }

  template<bool UseConst> Iterator<UseConst> end_impl() const
  {
    const int64_t span_num = this->get_chunk_num();
    BLI_assert(span_num >= 1);
    const Span<T> last_span = this->get_chunk(span_num - 1);

    Iterator<UseConst> it;
    it.chunk_list_ = const_cast<ChunkList *>(this);
    it.chunk_index_ = span_num;
    it.chunk_num_ = span_num;
    it.begin_ = const_cast<T *>(last_span.data());
    it.end_ = it.begin_ + last_span.size();
    it.current_ = it.end_;
    return it;
  }

  void ensure_space_for_one()
  {
    if (active_end_ >= active_capacity_end_) {
      this->activate_next_chunk();
    }
  }

  void activate_next_chunk()
  {
    if (alloc_info_ == nullptr) {
      this->prepare_alloc_info();
    }

    RawChunk &old_active_chunk = alloc_info_->raw_chunks[alloc_info_->active];
    old_active_chunk.end_if_inactive = active_end_;
    BLI_assert(old_active_chunk.capacity_end == active_capacity_end_);

    alloc_info_->active++;
    if (alloc_info_->active == alloc_info_->raw_chunks.size()) {
      this->add_chunk(1);
    }

    RawChunk &new_active_chunk = alloc_info_->raw_chunks[alloc_info_->active];
    active_begin_ = new_active_chunk.begin;
    active_end_ = new_active_chunk.end_if_inactive;
    active_capacity_end_ = new_active_chunk.capacity_end;
  }

  BLI_NOINLINE void prepare_alloc_info()
  {
    BLI_assert(alloc_info_ == nullptr);
    void *buffer = allocator_.allocate(sizeof(AllocInfo), alignof(AllocInfo), __func__);
    alloc_info_ = new (buffer) AllocInfo();
    alloc_info_->raw_chunks.append_as(inline_buffer_, active_end_, active_capacity_end_, nullptr);
    alloc_info_->active = 0;
  }

  BLI_NOINLINE void add_chunk(const int64_t min_chunk_size)
  {
    const RawChunk &last_chunk = alloc_info_->raw_chunks.last();
    const int64_t last_chunk_size = last_chunk.capacity_end - last_chunk.begin;
    const int64_t new_chunk_size = std::max<int64_t>(min_chunk_size,
                                                     std::min<int64_t>(last_chunk_size * 2, 4096));
    void *buffer = allocator_.allocate(
        sizeof(T) * static_cast<size_t>(new_chunk_size), alignof(T), __func__);
    T *new_chunk_begin = static_cast<T *>(buffer);
    T *new_chunk_capacity_end = new_chunk_begin + new_chunk_size;
    alloc_info_->raw_chunks.append_as(
        new_chunk_begin, new_chunk_begin, new_chunk_capacity_end, buffer);
  }
};

}  // namespace blender
