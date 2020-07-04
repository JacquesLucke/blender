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
 *
 * Some of the functions below have very similar alternatives in the standard library. However, it
 * is rather annoying to use those when debugging. Therefore, some more specialized and easier to
 * debug functions are provided here.
 */

#include <memory>
#include <new>

#include "BLI_utildefines.h"

namespace blender {

/**
 * Call the destructor on n consecutive values. For trivially destructible types, this does
 * nothing.
 *
 * Exception Safety: Destructors shouldn't really throw exceptions.
 *
 * Before:
 *  ptr: initialized
 * After:
 *  ptr: uninitialized
 */
template<typename T> void destruct_n(T *ptr, uint n) noexcept(std::is_nothrow_destructible_v<T>)
{
  /* This is not strictly necessary, because the loop below will be optimized away anyway. It is
   * nice to make behavior this explicitly, though. */
  if (std::is_trivially_destructible<T>::value) {
    return;
  }

  for (uint i = 0; i < n; i++) {
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
template<typename T> void default_construct_n(T *ptr, uint n)
{
  /* This is not strictly necessary, because the loop below will be optimized away anyway. It is
   * nice to make behavior this explicitly, though. */
  if (std::is_trivially_constructible<T>::value) {
    return;
  }

  uint current = 0;
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
template<typename T> void initialized_copy_n(const T *src, uint n, T *dst)
{
  for (uint i = 0; i < n; i++) {
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
template<typename T> void uninitialized_copy_n(const T *src, uint n, T *dst)
{
  uint current = 0;
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
template<typename T> void initialized_move_n(T *src, uint n, T *dst)
{
  for (uint i = 0; i < n; i++) {
    dst[i] = std::move(src[i]);
  }
}

/**
 * Move n values from src to dst.
 *
 * Exception Safety: Move constructors should not throw exceptions.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized, moved-from
 *  dst: initialized
 */
template<typename T>
void uninitialized_move_n(T *src, uint n, T *dst) noexcept(std::is_nothrow_move_constructible_v<T>)
{
  for (uint i = 0; i < n; i++) {
    new ((void *)(dst + i)) T(std::move(src[i]));
  }
}

/**
 * Relocate n values from src to dst. Relocation is a move followed by destruction of the src
 * value.
 *
 * Exception Safety: Relocation should not throw exceptions.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: uninitialized
 *  dst: initialized
 */
template<typename T> void initialized_relocate_n(T *src, uint n, T *dst)
{
  initialized_move_n(src, n, dst);
  destruct_n(src, n);
}

/**
 * Relocate n values from src to dst. Relocation is a move followed by destruction of the src
 * value.
 *
 * Exception Safety: Relocation should not throw exceptions.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: uninitialized
 *  dst: initialized
 */
template<typename T> void uninitialized_relocate_n(T *src, uint n, T *dst)
{
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
template<typename T> void initialized_fill_n(T *dst, uint n, const T &value)
{
  for (uint i = 0; i < n; i++) {
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
template<typename T> void uninitialized_fill_n(T *dst, uint n, const T &value)
{
  uint current = 0;
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
 * An `AlignedBuffer` is simply a byte array with the given size and alignment. The buffer will
 * not be initialized by the default constructor.
 *
 * This can be used to reserve memory for C++ objects whose lifetime is different from the
 * lifetime of the object they are embedded in. It's used by containers with small buffer
 * optimization and hash table implementations.
 */
template<size_t Size, size_t Alignment> class alignas(Alignment) AlignedBuffer {
 private:
  /* Don't create an empty array. This causes problems with some compilers. */
  char buffer_[(Size > 0) ? Size : 1];

 public:
  void *ptr()
  {
    return (void *)buffer_;
  }

  const void *ptr() const
  {
    return (const void *)buffer_;
  }
};

template<typename T, size_t Size = 1> class TypedAlignedBuffer {
 private:
  AlignedBuffer<sizeof(T) * Size, alignof(T)> buffer_;

 public:
  operator T *()
  {
    return this->ptr();
  }

  operator const T *() const
  {
    return this->ptr();
  }

  T *ptr()
  {
    return (T *)buffer_.ptr();
  }

  const T *ptr() const
  {
    return (const T *)buffer_.ptr();
  }
};

/**
 * This can be used by container constructors. A parameter of this type should be used to indicate
 * that the constructor does not construct the elements.
 */
class NoInitialization {
};

/**
 * This can be used to mark a constructor of an object that does not throw exceptions. Other
 * constructors can delegate to this constructor to make sure that the object lifetime starts.
 * With this, the destructor of the object will be called, even when the remaining constructor
 * throws.
 */
class NoExceptConstructor {
};

}  // namespace blender

#endif /* __BLI_MEMORY_UTILS_HH__ */
