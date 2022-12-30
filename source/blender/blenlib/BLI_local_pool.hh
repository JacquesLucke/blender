/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <cstddef>

#include "BLI_allocator.hh"
#include "BLI_asan.h"
#include "BLI_map.hh"
#include "BLI_math_bits.h"
#include "BLI_stack.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

class LocalPoolScope {
};

template<typename Allocator = GuardedAllocator> class LocalPool : NonCopyable, NonMovable {
 private:
  static constexpr int64_t s_alignment = 64;

  const LocalPoolScope &pool_scope_;
  Vector<MutableSpan<std::byte>> owned_buffers_;

  struct BufferStack {
    int64_t element_size = -1;
    Stack<void *, 0> stack;
  };

  std::array<BufferStack, 8> small_stacks_;
  std::unique_ptr<Map<int, BufferStack>> large_stacks_;

  BLI_NO_UNIQUE_ADDRESS Allocator allocator_;

 public:
  LocalPool(const LocalPoolScope &pool_scope) : pool_scope_(pool_scope)
  {
    for (const int64_t i : IndexRange(small_stacks_.size())) {
      small_stacks_[i].element_size = 8 * (i + 1);
    }
  }

  ~LocalPool()
  {
    for (MutableSpan<std::byte> buffer : owned_buffers_) {
      BLI_asan_unpoison(buffer.data(), buffer.size());
      allocator_.deallocate(buffer.data());
    }
  }

  void *allocate(const int64_t size, const int64_t alignment)
  {
    BLI_assert((size == 0 || alignment <= size) && alignment <= s_alignment);
    BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
    if (!buffer_stack.stack.is_empty()) {
      void *buffer = buffer_stack.stack.pop();
      BLI_asan_unpoison(buffer, size);
      return buffer;
    }
    if (size <= 4096) {
      const int64_t allocation_size = std::clamp<int64_t>(
          buffer_stack.element_size * 16, 512, 4096);
      void *buffer = allocator_.allocate(allocation_size, s_alignment, __func__);
      BLI_asan_poison(buffer, allocation_size);
      const int64_t num = allocation_size / buffer_stack.element_size;
      for (int64_t i = num - 1; i > 0; i--) {
        buffer_stack.stack.push(POINTER_OFFSET(buffer, buffer_stack.element_size * i));
      }
      owned_buffers_.append({static_cast<std::byte *>(buffer), allocation_size});
      BLI_asan_unpoison(buffer, size);
      return buffer;
    }
    void *buffer = allocator_.allocate(
        size_t(size), std::max<size_t>(s_alignment, size_t(alignment)), __func__);
    owned_buffers_.append({static_cast<std::byte *>(buffer), size});
    return buffer;
  }

  void deallocate(const void *buffer, const int64_t size, const int64_t alignment)
  {
    BLI_assert((size == 0 || alignment <= size) && alignment <= s_alignment);
#ifdef DEBUG
    memset(buffer, -1, size);
#endif
    BLI_asan_poison(buffer, alignment);
    BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
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

}  // namespace blender
