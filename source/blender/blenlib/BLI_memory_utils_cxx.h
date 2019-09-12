#pragma once

#include <memory>
#include <algorithm>

namespace BLI {

using std::copy;
using std::copy_n;
using std::uninitialized_copy;
using std::uninitialized_copy_n;
using std::uninitialized_fill;
using std::uninitialized_fill_n;

template<typename T> void destruct(T *ptr)
{
  ptr->~T();
}

template<typename T> void destruct_n(T *ptr, uint n)
{
  for (uint i = 0; i < n; i++) {
    ptr[i].~T();
  }
}

template<typename T> void uninitialized_move_n(T *src, uint n, T *dst)
{
  std::uninitialized_copy_n(std::make_move_iterator(src), n, dst);
}

template<typename T> void move_n(T *src, uint n, T *dst)
{
  std::copy_n(std::make_move_iterator(src), n, dst);
}

template<typename T> void uninitialized_relocate(T *src, T *dst)
{
  new (dst) T(std::move(*src));
  destruct(src);
}

template<typename T> void uninitialized_relocate_n(T *src, uint n, T *dst)
{
  uninitialized_move_n(src, n, dst);
  destruct_n(src, n);
}

template<typename T> void relocate(T *src, T *dst)
{
  *dst = std::move(*src);
  destruct(src);
}

template<typename T> void relocate_n(T *src, uint n, T *dst)
{
  move_n(src, n, dst);
  destruct_n(src, n);
}

}  // namespace BLI
