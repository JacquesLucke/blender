/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BLI_MEMORY_UTILS_HH__
#define __BLI_MEMORY_UTILS_HH__

/** \file
 * \ingroup bli
 * Some of the functions below have very similar alternatives in the standard library. However, it
 * is rather annoying to use those when debugging. Therefore, some more specialized and easier to
 * debug functions are provided here.
 */

#include <memory>
#include <new>
#include <type_traits>

#include "BLI_utildefines.h"

namespace blender {

/**
 * Call the destructor on n consecutive values. For trivially destructible types, this does
 * nothing.
 *
 * Exception Safety: Destructors shouldn't throw exceptions.
 *
 * Before:
 *  ptr: initialized
 * After:
 *  ptr: uninitialized
 */
template<typename T> void destruct_n(T *ptr, int64_t n)
{
  BLI_assert(n >= 0);

  static_assert(std::is_nothrow_destructible_v<T>,
                "This should be true for all types. Destructors are noexcept by default.");

  /* This is not strictly necessary, because the loop below will be optimized away anyway. It is
   * nice to make behavior this explicitly, though. */
  if (std::is_trivially_destructible_v<T>) {
    return;
  }

  for (int64_t i = 0; i < n; i++) {
    ptr[i].~T();
  }
}

/**
 * Call the default constructor on n consecutive elements. For trivially constructible types, this
 * does nothing.
 *
 * Exception Safety: Strong.
 *
 * Before:
 *  ptr: uninitialized
 * After:
 *  ptr: initialized
 */
template<typename T> void default_construct_n(T *ptr, int64_t n)
{
  BLI_assert(n >= 0);

  /* This is not strictly necessary, because the loop below will be optimized away anyway. It is
   * nice to make behavior this explicitly, though. */
  if (std::is_trivially_constructible_v<T>) {
    return;
  }

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new ((void *)(ptr + current)) T;
    }
  }
  catch (...) {
    destruct_n(ptr, current);
    throw;
  }
}

/**
 * Copy n values from src to dst.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: initialized
 *  dst: initialized
 */
template<typename T> void initialized_copy_n(const T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  for (int64_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
}

/**
 * Copy n values from src to dst.
 *
 * Exception Safety: Strong.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized
 *  dst: initialized
 */
template<typename T> void uninitialized_copy_n(const T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new ((void *)(dst + current)) T(src[current]);
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

/**
 * Convert n values from type `From` to type `To`.
 *
 * Exception Safety: Strong.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized
 *  dst: initialized
 */
template<typename From, typename To>
void uninitialized_convert_n(const From *src, int64_t n, To *dst)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new ((void *)(dst + current)) To((To)src[current]);
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

/**
 * Move n values from src to dst.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: initialized, moved-from
 *  dst: initialized
 */
template<typename T> void initialized_move_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  for (int64_t i = 0; i < n; i++) {
    dst[i] = std::move(src[i]);
  }
}

/**
 * Move n values from src to dst.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized, moved-from
 *  dst: initialized
 */
template<typename T> void uninitialized_move_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new ((void *)(dst + current)) T(std::move(src[current]));
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

/**
 * Relocate n values from src to dst. Relocation is a move followed by destruction of the src
 * value.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: uninitialized
 *  dst: initialized
 */
template<typename T> void initialized_relocate_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  initialized_move_n(src, n, dst);
  destruct_n(src, n);
}

/**
 * Relocate n values from src to dst. Relocation is a move followed by destruction of the src
 * value.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: uninitialized
 *  dst: initialized
 */
template<typename T> void uninitialized_relocate_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  uninitialized_move_n(src, n, dst);
  destruct_n(src, n);
}

/**
 * Copy the value to n consecutive elements.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  dst: initialized
 * After:
 *  dst: initialized
 */
template<typename T> void initialized_fill_n(T *dst, int64_t n, const T &value)
{
  BLI_assert(n >= 0);

  for (int64_t i = 0; i < n; i++) {
    dst[i] = value;
  }
}

/**
 * Copy the value to n consecutive elements.
 *
 *  Exception Safety: Strong.
 *
 * Before:
 *  dst: uninitialized
 * After:
 *  dst: initialized
 */
template<typename T> void uninitialized_fill_n(T *dst, int64_t n, const T &value)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new ((void *)(dst + current)) T(value);
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

template<typename T> struct DestructValueAtAddress {
  void operator()(T *ptr)
  {
    ptr->~T();
  }
};

/**
 * A destruct_ptr is like unique_ptr, but it will only call the destructor and will not free the
 * memory. This is useful when using custom allocators.
 */
template<typename T> using destruct_ptr = std::unique_ptr<T, DestructValueAtAddress<T>>;

/**
 * An `AlignedBuffer` is a byte array with at least the given size and alignment. The buffer will
 * not be initialized by the default constructor.
 */
template<size_t Size, size_t Alignment> class alignas(Alignment) AlignedBuffer {
 private:
  /* Don't create an empty array. This causes problems with some compilers. */
  char buffer_[(Size > 0) ? Size : 1];

 public:
  operator void *()
  {
    return (void *)buffer_;
  }

  operator const void *() const
  {
    return (void *)buffer_;
  }

  void *ptr()
  {
    return (void *)buffer_;
  }

  const void *ptr() const
  {
    return (const void *)buffer_;
  }
};

class EmptyAlignedBuffer {
 public:
  operator void *()
  {
    return nullptr;
  }

  operator const void *() const
  {
    return nullptr;
  }

  void *ptr()
  {
    return nullptr;
  }

  const void *ptr() const
  {
    return nullptr;
  }
};

template<size_t Size, size_t Alignment>
using MaybeEmptyAlignedBuffer =
    std::conditional_t<Size == 0, EmptyAlignedBuffer, AlignedBuffer<Size, Alignment>>;

/**
 * This can be used to reserve memory for C++ objects whose lifetime is different from the
 * lifetime of the object they are embedded in. It's used by containers with small buffer
 * optimization and hash table implementations.
 */
template<typename T, int64_t Size = 1>
class TypedBuffer : private MaybeEmptyAlignedBuffer<sizeof(T) * (size_t)Size, alignof(T)> {
 public:
  operator void *()
  {
    return this;
  }

  operator const void *() const
  {
    return this;
  }

  operator T *()
  {
    return (T *)this;
  }

  operator const T *() const
  {
    return (const T *)this;
  }

  T &operator*()
  {
    return *(T *)this;
  }

  const T &operator*() const
  {
    return *(const T *)this;
  }

  T *ptr()
  {
    return (T *)this;
  }

  const T *ptr() const
  {
    return (const T *)this;
  }

  T &ref()
  {
    return *(T *)this;
  }

  const T &ref() const
  {
    return *(const T *)this;
  }
};

template<typename T1, typename T2> class CompressedPair_BothEmpty : private T1, private T2 {
 public:
  CompressedPair_BothEmpty() = default;

  template<typename U1, typename U2>
  CompressedPair_BothEmpty(U1 &&UNUSED(value1), U2 &&UNUSED(value2))
  {
  }

  T1 &first()
  {
    return *this;
  }

  const T1 &first() const
  {
    return *this;
  }

  T2 &second()
  {
    return *this;
  }

  const T2 &second() const
  {
    return *this;
  }
};

template<typename T1, typename T2> class CompressedPair_FirstEmpty : private T1 {
 private:
  T2 second_;

 public:
  CompressedPair_FirstEmpty() = default;

  template<typename U1, typename U2>
  CompressedPair_FirstEmpty(U1 &&UNUSED(value1), U2 &&value2) : second_(std::forward<U2>(value2))
  {
  }

  T1 &first()
  {
    return *this;
  }

  const T1 &first() const
  {
    return *this;
  }

  T2 &second()
  {
    return second_;
  }

  const T2 &second() const
  {
    return second_;
  }
};

template<typename T1, typename T2> class CompressedPair_SecondEmpty : private T2 {
 private:
  T1 first_;

 public:
  CompressedPair_SecondEmpty() = default;

  template<typename U1, typename U2>
  CompressedPair_SecondEmpty(U1 &&value1, U2 &&UNUSED(value2)) : first_(std::forward<U1>(value1))
  {
  }

  T1 &first()
  {
    return first_;
  }

  const T1 &first() const
  {
    return first_;
  }

  T2 &second()
  {
    return *this;
  }

  const T2 &second() const
  {
    return *this;
  }
};

template<typename T1, typename T2> class CompressedPair_NoneEmpty {
 private:
  T1 first_;
  T2 second_;

 public:
  CompressedPair_NoneEmpty() = default;

  template<typename U1, typename U2>
  CompressedPair_NoneEmpty(U1 &&value1, U2 &&value2)
      : first_(std::forward<U1>(value1)), second_(std::forward<U2>(value2))
  {
  }

  T1 &first()
  {
    return first_;
  }

  const T1 &first() const
  {
    return first_;
  }

  T2 &second()
  {
    return second_;
  }

  const T2 &second() const
  {
    return second_;
  }
};

template<typename T1, typename T2>
using CompressedPair = std::conditional_t<std::is_empty_v<T1>,
                                          std::conditional_t<std::is_empty_v<T2>,
                                                             CompressedPair_BothEmpty<T1, T2>,
                                                             CompressedPair_FirstEmpty<T1, T2>>,
                                          std::conditional_t<std::is_empty_v<T2>,
                                                             CompressedPair_SecondEmpty<T1, T2>,
                                                             CompressedPair_NoneEmpty<T1, T2>>>;

template<typename T1, typename T2, typename T3>
using CompressedTripleBase = CompressedPair<T1, CompressedPair<T2, T3>>;

template<typename T1, typename T2, typename T3>
class CompressedTriple : private CompressedTripleBase<T1, T2, T3> {
 private:
  using Base = CompressedTripleBase<T1, T2, T3>;

 public:
  CompressedTriple() = default;

  template<typename U1, typename U2, typename U3>
  CompressedTriple(U1 &&value1, U2 &&value2, U3 &&value3)
      : Base(std::forward<U1>(value1),
             CompressedPair<T2, T3>(std::forward<U2>(value2), std::forward<U3>(value3)))
  {
  }

  CompressedTriple(T1 value1, T2 value2, T3 value3)
      : Base(std::move(value1), CompressedPair<T2, T3>(std::move(value2), std::move(value3)))
  {
  }

  T1 &first()
  {
    return Base::first();
  }

  const T1 &first() const
  {
    return Base::first();
  }

  T2 &second()
  {
    return Base::second().first();
  }

  const T2 &second() const
  {
    return Base::second().first();
  }

  T3 &third()
  {
    return Base::second().second();
  }

  const T3 &third() const
  {
    return Base::second().second();
  }
};

/**
 * This can be used by container constructors. A parameter of this type should be used to indicate
 * that the constructor does not construct the elements.
 */
class NoInitialization {
};

/**
 * Helper variable that checks if a pointer type can be converted into another pointer type without
 * issues. Possible issues are casting away const and casting a pointer to a child class.
 * Adding const or casting to a parent class is fine.
 */
template<typename From, typename To>
inline constexpr bool is_convertible_pointer_v =
    std::is_convertible_v<From, To> &&std::is_pointer_v<From> &&std::is_pointer_v<To>;

/**
 * Inline buffers for small-object-optimization should be disable by default. Otherwise we might
 * get large unexpected allocations on the stack.
 */
inline constexpr int64_t default_inline_buffer_capacity(size_t element_size)
{
  return ((int64_t)element_size < 100) ? 4 : 0;
}

}  // namespace blender

#endif /* __BLI_MEMORY_UTILS_HH__ */
