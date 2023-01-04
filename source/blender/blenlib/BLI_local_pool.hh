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
    Stack<void *, 0> stack;
  };

  std::array<BufferStack, 8> small_stacks_;
  std::unique_ptr<Map<int, BufferStack>> large_stacks_;

 public:
  LocalPool()
  {
    for (const int64_t i : IndexRange(small_stacks_.size())) {
      small_stacks_[i].element_size = 8 * (i + 1);
    }
  }

  ~LocalPool()
  {
  }

  void *allocate(const int64_t size, const int64_t alignment)
  {
    BLI_assert((size == 0 || alignment <= size) && alignment <= s_alignment);
    BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
    BLI_assert(buffer_stack.element_size >= size);
    if (!buffer_stack.stack.is_empty()) {
      void *buffer = buffer_stack.stack.pop();
      BLI_asan_unpoison(buffer, size);
      return buffer;
    }
    if (size <= 4096) {
      return linear_allocator_.allocate(size, alignment);
    }
    return linear_allocator_.allocate(size_t(size),
                                      std::max<size_t>(s_alignment, size_t(alignment)));
  }

  void deallocate(const void *buffer, const int64_t size, const int64_t alignment)
  {
    BLI_assert((size == 0 || alignment <= size) && alignment <= s_alignment);
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

 private:
  BufferStack &get_buffer_stack(const int64_t size, const int64_t /*alignment*/)
  {
    if (size <= 64) {
      return small_stacks_[(size - (size != 0)) >> 3];
    }
    if (!large_stacks_) {
      large_stacks_ = std::make_unique<Map<int, BufferStack>>();
    }
    const int key = bitscan_reverse_uint64(uint64_t(size));
    return large_stacks_->lookup_or_add_cb(key, [&]() {
      BufferStack buffer_stack;
      buffer_stack.element_size = int64_t(1) << (8 * sizeof(int64_t) - key);
      return buffer_stack;
    });
  }
};

class LocalMemoryPools {
 private:
  threading::EnumerableThreadSpecific<LocalPool<>> pool_by_thread_;

 public:
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
