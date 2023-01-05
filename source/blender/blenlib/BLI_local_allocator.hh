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
class LocalAllocator;
class LocalAllocatorPool;

class LocalAllocatorPool : NonCopyable, NonMovable {
 private:
  Stack<void *> buffers;
  int64_t element_size = -1;
  int64_t alignment = -1;

  friend LocalAllocator;
};

class LocalAllocator : NonCopyable, NonMovable {
 private:
  static constexpr int64_t s_alignment = 64;
  LocalAllocatorSet &owner_set_;
  LinearAllocator<> linear_allocator_;

  struct Head {
    int64_t buffer_size;
    int64_t buffer_alignment;
  };
  static_assert(is_power_of_2_constexpr(sizeof(Head)));

  std::array<LocalAllocatorPool, 8> small_buffer_pools_;
  Map<int, std::unique_ptr<LocalAllocatorPool>> large_buffer_pools_;

  friend LocalAllocatorSet;

  LocalAllocator(LocalAllocatorSet &owner_set);

 public:
  ~LocalAllocator();

  bool is_local() const;
  LocalAllocator &local();
  LocalAllocatorSet &owner_set();

  void *allocate(int64_t size, int64_t alignment);
  void deallocate(const void *buffer, int64_t size, int64_t alignment);

  void *allocate(LocalAllocatorPool &pool);
  void deallocate(const void *buffer, LocalAllocatorPool &pool);

  void *allocate_with_head(int64_t size, int64_t alignment);
  void deallocate_with_head(const void *buffer);

  LocalAllocatorPool &get_pool(int64_t size, int64_t alignment);

  template<typename T, typename... Args> T &allocate_new(Args &&...args);
  template<typename T, typename... Args> void destruct_free(const T *value);
  template<typename T> MutableSpan<T> allocate_array(int64_t size);
  template<typename T, typename... Args>
  MutableSpan<T> allocate_new_array(int64_t size, Args &&...args);
  template<typename T> void destruct_free_array(Span<T> data);
  template<typename T> void destruct_free_array(MutableSpan<T> data);
};

class LocalAllocatorSet : NonCopyable, NonMovable {
 private:
  threading::EnumerableThreadSpecific<LocalAllocator> allocator_by_thread_;

#ifdef BLI_LOCAL_ALLOCATOR_DEBUG_SIZES
  std::mutex debug_sizes_mutex_;
  Map<const void *, int64_t> debug_sizes_;
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
  LocalAllocatorPool &pool = this->get_pool(size, alignment);
  BLI_assert(pool.element_size >= size);
  BLI_assert(pool.alignment >= alignment);

  return this->allocate(pool);
}

BLI_NOINLINE inline void LocalAllocator::deallocate(const void *buffer,
                                                    const int64_t size,
                                                    const int64_t alignment)
{
  LocalAllocatorPool &pool = this->get_pool(size, alignment);
  BLI_assert(pool.element_size >= size);
  BLI_assert(pool.alignment >= alignment);

  this->deallocate(buffer, pool);
}

inline void *LocalAllocator::allocate(LocalAllocatorPool &pool)
{
  BLI_assert(this->is_local());

#ifdef BLI_LOCAL_ALLOCATOR_USE_GUARDED
  return MEM_mallocN_aligned(size, alignment, __func__);
#endif

  void *buffer;
  if (!pool.buffers.is_empty()) {
    buffer = pool.buffers.pop();
    BLI_asan_unpoison(buffer, pool.element_size);
  }
  else {
    buffer = linear_allocator_.allocate(pool.element_size, pool.alignment);
  }

#ifdef BLI_LOCAL_ALLOCATOR_DEBUG_SIZES
  {
    std::lock_guard lock{owner_set_.debug_sizes_mutex_};
    owner_set_.debug_sizes_.add_new(buffer, pool.element_size);
  }
#endif

  return buffer;
}

inline void LocalAllocator::deallocate(const void *buffer, LocalAllocatorPool &pool)
{
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
  memset(const_cast<void *>(buffer), -1, pool.element_size);
#endif

  BLI_asan_poison(buffer, pool.element_size);

  pool.buffers.push(const_cast<void *>(buffer));
}

inline LocalAllocatorPool &LocalAllocator::get_pool(const int64_t size, const int64_t alignment)
{
  BLI_assert(size > 0);
  BLI_assert(alignment <= size);
  BLI_assert(alignment <= s_alignment);
  BLI_assert(is_power_of_2_i(alignment));
  UNUSED_VARS_NDEBUG(alignment);

  BLI_assert(this->is_local());
  if (size <= 64) {
    return small_buffer_pools_[(size - 1) >> 3];
  }
  const int key = bitscan_reverse_uint64(uint64_t(size));
  return *large_buffer_pools_.lookup_or_add_cb(key, [&]() {
    auto pool = std::make_unique<LocalAllocatorPool>();
    pool->element_size = int64_t(1) << (8 * sizeof(int64_t) - key);
    pool->alignment = s_alignment;
    return pool;
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
