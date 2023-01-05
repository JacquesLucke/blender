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

// #define BLI_LOCAL_ALLOCATOR_USE_GUARDED
// #define BLI_LOCAL_ALLOCATOR_DEBUG_SIZES

namespace blender {

class LocalAllocatorSet;

class LocalAllocator : NonCopyable, NonMovable {
 private:
  static constexpr int64_t s_alignment = 64;
  LocalAllocatorSet &owner_set_;
  LinearAllocator<> linear_allocator_;

  struct BufferStack {
    Stack<void *, 0> stack;
    int64_t element_size = -1;
    int64_t alignment = -1;
  };

  struct Head {
    int64_t buffer_size;
    int64_t buffer_alignment;
  };
  static_assert(is_power_of_2_constexpr(sizeof(Head)));

  std::array<BufferStack, 8> small_stacks_;
  Map<int, BufferStack> large_stacks_;

  friend LocalAllocatorSet;

  LocalAllocator(LocalAllocatorSet &owner_set);

 public:
  ~LocalAllocator();

  bool is_local() const;
  LocalAllocator &local();
  LocalAllocatorSet &owner_set();

  void *allocate(int64_t size, int64_t alignment);
  void deallocate(const void *buffer, int64_t size, int64_t alignment);

  void *allocate_with_head(int64_t size, int64_t alignment);
  void deallocate_with_head(const void *buffer);

  template<typename T, typename... Args> T &allocate_new(Args &&...args);
  template<typename T, typename... Args> void destruct_free(const T *value);
  template<typename T> MutableSpan<T> allocate_array(int64_t size);
  template<typename T, typename... Args>
  MutableSpan<T> allocate_new_array(int64_t size, Args &&...args);
  template<typename T> void destruct_free_array(Span<T> data);
  template<typename T> void destruct_free_array(MutableSpan<T> data);

 private:
  BufferStack &get_buffer_stack(int64_t size, int64_t alignment);
};

class LocalAllocatorSet : NonCopyable, NonMovable {
 private:
  threading::EnumerableThreadSpecific<LocalAllocator> allocator_by_thread_;

#ifdef BLI_LOCAL_ALLOCATOR_DEBUG_SIZES
  std::mutex debug_sizes_mutex_;
  Map<const void *, std::pair<int64_t, int64_t>> debug_sizes_;
#endif

  friend LocalAllocator;

 public:
  LocalAllocatorSet();
  ~LocalAllocatorSet();

  LocalAllocator &local();
};

class ThreadedLocalAllocatorRef {
 private:
  LocalAllocatorSet &allocator_set_;

 public:
  ThreadedLocalAllocatorRef(LocalAllocator &allocator) : allocator_set_(allocator.owner_set())
  {
  }

  void *allocate(const size_t size, const size_t alignment, const char * /*name*/)
  {
    LocalAllocator &allocator = allocator_set_.local();
    return allocator.allocate_with_head(size, alignment);
  }

  void deallocate(void *ptr)
  {
    LocalAllocator &allocator = allocator_set_.local();
    allocator.deallocate_with_head(ptr);
  }
};

class LocalAllocatorRef {
 private:
  LocalAllocator &allocator_;

 public:
  LocalAllocatorRef(LocalAllocator &allocator) : allocator_(allocator)
  {
  }

  void *allocate(const size_t size, const size_t alignment, const char * /*name*/)
  {
    return allocator_.allocate_with_head(size, alignment);
  }

  void deallocate(void *ptr)
  {
    allocator_.deallocate_with_head(ptr);
  }
};

inline bool LocalAllocator::is_local() const
{
  return this == &owner_set_.local();
}

inline LocalAllocator &LocalAllocator::local()
{
  return owner_set_.local();
}

inline LocalAllocatorSet &LocalAllocator::owner_set()
{
  return owner_set_;
}

BLI_NOINLINE inline void *LocalAllocator::allocate(const int64_t size, const int64_t alignment)
{
  BLI_assert(size > 0);
  BLI_assert(alignment <= size);
  BLI_assert(alignment <= s_alignment);
  BLI_assert(is_power_of_2_i(alignment));
  BLI_assert(this->is_local());

#ifdef BLI_LOCAL_ALLOCATOR_USE_GUARDED
  return MEM_mallocN_aligned(size, alignment, __func__);
#endif

  BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
  BLI_assert(buffer_stack.element_size >= size);
  BLI_assert(buffer_stack.alignment >= alignment);

  void *buffer;
  if (!buffer_stack.stack.is_empty()) {
    buffer = buffer_stack.stack.pop();
    BLI_asan_unpoison(buffer, size);
  }
  else {
    buffer = linear_allocator_.allocate(buffer_stack.element_size, buffer_stack.alignment);
  }

#ifdef BLI_LOCAL_ALLOCATOR_DEBUG_SIZES
  {
    std::lock_guard lock{owner_set_.debug_sizes_mutex_};
    owner_set_.debug_sizes_.add_new(buffer, {size, alignment});
  }
#endif

  return buffer;
}

BLI_NOINLINE inline void LocalAllocator::deallocate(const void *buffer,
                                                    const int64_t size,
                                                    const int64_t alignment)
{
  BLI_assert(size > 0);
  BLI_assert(alignment <= size);
  BLI_assert(alignment <= s_alignment);
  BLI_assert(is_power_of_2_i(alignment));
  BLI_assert(this->is_local());

#ifdef BLI_LOCAL_ALLOCATOR_USE_GUARDED
  MEM_freeN(const_cast<void *>(buffer));
  UNUSED_VARS_NDEBUG(size, alignment);
  return;
#endif

#ifdef BLI_LOCAL_ALLOCATOR_DEBUG_SIZES
  {
    std::lock_guard lock{owner_set_.debug_sizes_mutex_};
    auto [last_size, last_alignment] = owner_set_.debug_sizes_.pop(buffer);
    if (last_size != size) {
      BLI_assert_unreachable();
    }
    if (last_alignment != alignment) {
      BLI_assert_unreachable();
    }
  }
#endif

#ifdef DEBUG
  memset(const_cast<void *>(buffer), -1, size);
#endif
  BLI_asan_poison(buffer, size);

  BufferStack &buffer_stack = this->get_buffer_stack(size, alignment);
  BLI_assert(buffer_stack.element_size >= size);
  BLI_assert(buffer_stack.alignment >= alignment);

  buffer_stack.stack.push(const_cast<void *>(buffer));
}

inline LocalAllocator::BufferStack &LocalAllocator::get_buffer_stack(const int64_t size,
                                                                     const int64_t /*alignment*/)
{
  if (size <= 64) {
    return small_stacks_[(size - 1) >> 3];
  }
  const int key = bitscan_reverse_uint64(uint64_t(size));
  return large_stacks_.lookup_or_add_cb(key, [&]() {
    BufferStack buffer_stack;
    buffer_stack.element_size = int64_t(1) << (8 * sizeof(int64_t) - key);
    buffer_stack.alignment = s_alignment;
    return buffer_stack;
  });
}

inline void *LocalAllocator::allocate_with_head(int64_t size, int64_t alignment)
{
  const int64_t buffer_size = size + std::max<int64_t>(alignment, sizeof(Head));
  const int64_t buffer_alignment = std::max<int64_t>(alignment, alignof(Head));
  void *buffer = this->allocate(buffer_size, buffer_alignment);
  Head *head = new (buffer) Head;
  head->buffer_size = buffer_size;
  head->buffer_alignment = buffer_alignment;
  return head + 1;
}

inline void LocalAllocator::deallocate_with_head(const void *buffer)
{
  const Head *head = static_cast<const Head *>(buffer) - 1;
  this->deallocate(head, head->buffer_size, head->buffer_alignment);
}

template<typename T, typename... Args> inline T &LocalAllocator::allocate_new(Args &&...args)
{
  void *buffer = this->allocate(sizeof(T), alignof(T));
  T *value = new (buffer) T(std::forward<Args>(args)...);
  return *value;
}

template<typename T, typename... Args> inline void LocalAllocator::destruct_free(const T *value)
{
  std::destroy_at(value);
  this->deallocate(value, sizeof(T), alignof(T));
}

template<typename T> MutableSpan<T> inline LocalAllocator::allocate_array(const int64_t size)
{
  if (size == 0) {
    return {};
  }
  void *buffer = this->allocate(size * sizeof(T), alignof(T));
  return {static_cast<T *>(buffer), size};
}

template<typename T, typename... Args>
MutableSpan<T> inline LocalAllocator::allocate_new_array(const int64_t size, Args &&...args)
{
  MutableSpan<T> array = this->allocate_array<T>(size);
  for (const int64_t i : IndexRange(size)) {
    new (&array[i]) T(std::forward<Args>(args)...);
  }
  return array;
}

template<typename T> inline void LocalAllocator::destruct_free_array(Span<T> data)
{
  if (data.is_empty()) {
    return;
  }
  destruct_n(const_cast<T *>(data.data()), data.size());
  this->deallocate(data.data(), data.size_in_bytes(), alignof(T));
}

template<typename T> inline void LocalAllocator::destruct_free_array(MutableSpan<T> data)
{
  this->destruct_free_array(data.as_span());
}

inline LocalAllocator &LocalAllocatorSet::local()
{
  return allocator_by_thread_.local();
}

}  // namespace blender
