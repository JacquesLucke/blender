/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <cstddef>

#include "BLI_allocator.hh"
#include "BLI_asan.h"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_math_bits.h"
#include "BLI_stack.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename Allocator = GuardedAllocator> class LocalPool : NonCopyable, NonMovable {
 private:
  static constexpr int64_t s_alignment = 64;
  LinearAllocator<> linear_allocator_;

  struct BufferStack {
    int64_t element_size = -1;
    int64_t min_alignment = -1;
    Stack<void *, 0> stack;
  };

  std::array<BufferStack, 8> small_stacks_;
  std::unique_ptr<Map<int, BufferStack>> large_stacks_;

 public:
  LocalPool()
  {
    for (const int64_t i : IndexRange(small_stacks_.size())) {
      BufferStack &buffer_stack = small_stacks_[i];
      buffer_stack.element_size = 8 * (i + 1);
      buffer_stack.min_alignment = power_of_2_min_u(buffer_stack.element_size);
    }
  }

  ~LocalPool()
  {
  }

  void *allocate(const int64_t size, const int64_t alignment)
  {
    BLI_assert(size > 0);
    BLI_assert(alignment <= size && alignment <= s_alignment);

    BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
    BLI_assert(buffer_stack.element_size >= size);
    BLI_assert(buffer_stack.min_alignment >= alignment);

    void *buffer;
    if (!buffer_stack.stack.is_empty()) {
      buffer = buffer_stack.stack.pop();
      BLI_asan_unpoison(buffer, size);
    }
    else if (size <= 4096) {
      buffer = linear_allocator_.allocate(buffer_stack.element_size, buffer_stack.min_alignment);
    }
    else {
      buffer = linear_allocator_.allocate(size_t(size),
                                          std::max<size_t>(s_alignment, size_t(alignment)));
    }
    return buffer;
  }

  void deallocate(const void *buffer, const int64_t size, const int64_t alignment)
  {
    BLI_assert(size > 0);
    BLI_assert(alignment <= size && alignment <= s_alignment);

#ifdef DEBUG
    memset(const_cast<void *>(buffer), -1, size);
#endif
    BLI_asan_poison(buffer, size);
    BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
    BLI_assert(buffer_stack.element_size >= size);
    buffer_stack.stack.push(const_cast<void *>(buffer));
  }

  template<typename T, typename... Args> destruct_ptr<T> construct(Args &&...args)
  {
    void *buffer = this->allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    return destruct_ptr<T>(value);
  }

  template<typename T> MutableSpan<T> allocate_array(int64_t size)
  {
    if (size == 0) {
      return {};
    }
    T *array = static_cast<T *>(this->allocate(sizeof(T) * size, alignof(T)));
    return MutableSpan<T>(array, size);
  }

  template<typename T, typename... Args>
  MutableSpan<T> construct_array(int64_t size, Args &&...args)
  {
    MutableSpan<T> array = this->allocate_array<T>(size);
    for (const int64_t i : IndexRange(size)) {
      new (&array[i]) T(std::forward<Args>(args)...);
    }
    return array;
  }

  template<typename T> void destruct_array(Span<T> data)
  {
    if (data.is_empty()) {
      return;
    }
    destruct_n(const_cast<T *>(data.data()), data.size());
    this->deallocate(data.data(), data.size() * sizeof(T), alignof(T));
  }

  template<typename T> void destruct_array(MutableSpan<T> data)
  {
    this->destruct_array(data.as_span());
  }

  template<typename T> void destruct(const T *value)
  {
    std::destroy_at(value);
    this->deallocate(value, sizeof(T), alignof(T));
  }

 private:
  BufferStack &get_buffer_stack(const int64_t size, const int64_t /*alignment*/)
  {
    if (size <= 64) {
      return small_stacks_[(size - 1) >> 3];
    }
    if (!large_stacks_) {
      large_stacks_ = std::make_unique<Map<int, BufferStack>>();
    }
    const int key = bitscan_reverse_uint64(uint64_t(size));
    return large_stacks_->lookup_or_add_cb(key, [&]() {
      BufferStack buffer_stack;
      buffer_stack.element_size = int64_t(1) << (8 * sizeof(int64_t) - key);
      buffer_stack.min_alignment = s_alignment;
      return buffer_stack;
    });
  }
};

class LocalMemoryPools {
 private:
  threading::EnumerableThreadSpecific<LocalPool<>> pool_by_thread_;

 public:
  ~LocalMemoryPools()
  {
  }

  LocalPool<> &local()
  {
    return pool_by_thread_.local();
  }
};

struct Pools {
  LocalMemoryPools *pools = nullptr;
  LocalPool<> *local = nullptr;
};

}  // namespace blender
