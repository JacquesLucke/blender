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

  std::array<BufferStack, 8> small_stacks_;
  Map<int, BufferStack> large_stacks_;

  friend LocalAllocatorSet;

  LocalAllocator(LocalAllocatorSet &owner_set);

 public:
  ~LocalAllocator();

  bool is_local() const;
  LocalAllocator &local();

  void *allocate(const int64_t size, const int64_t alignment);
  void deallocate(const void *buffer, const int64_t size, const int64_t alignment);

  template<typename T, typename... Args> T &allocate_new(Args &&...args);
  template<typename T, typename... Args> void destruct_free(const T *value);
  template<typename T> MutableSpan<T> allocate_array(const int64_t size);
  template<typename T, typename... Args>
  MutableSpan<T> allocate_new_array(const int64_t size, Args &&...args);
  template<typename T> void destruct_free_array(Span<T> data);
  template<typename T> void destruct_free_array(MutableSpan<T> data);

 private:
  BufferStack &get_buffer_stack(const int64_t size, const int64_t alignment);
};

class LocalAllocatorSet : NonCopyable, NonMovable {
 private:
  threading::EnumerableThreadSpecific<LocalAllocator> allocator_by_thread_;

 public:
  LocalAllocatorSet();
  ~LocalAllocatorSet();

  LocalAllocator &local();
};

inline bool LocalAllocator::is_local() const
{
  return this == &owner_set_.local();
}

inline LocalAllocator &LocalAllocator::local()
{
  return owner_set_.local();
}

inline void *LocalAllocator::allocate(const int64_t size, const int64_t alignment)
{
  BLI_assert(size > 0);
  BLI_assert(alignment <= size);
  BLI_assert(alignment <= s_alignment);
  BLI_assert(is_power_of_2_i(alignment));
  BLI_assert(this->is_local());

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
  return buffer;
}

inline void LocalAllocator::deallocate(const void *buffer,
                                       const int64_t size,
                                       const int64_t alignment)
{
  BLI_assert(size > 0);
  BLI_assert(alignment <= size);
  BLI_assert(alignment <= s_alignment);
  BLI_assert(is_power_of_2_i(alignment));
  BLI_assert(this->is_local());

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
