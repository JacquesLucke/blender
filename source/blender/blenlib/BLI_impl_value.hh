/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_memory_utils.hh"

namespace blender {

template<typename T, size_t MaxSize, size_t MaxAlignment> class ImplValue {
 private:
  AlignedBuffer<MaxSize, MaxAlignment> buffer_;

 public:
#define BLI_impl_value_type_check() \
  static_assert(sizeof(T) <= MaxSize && alignof(T) <= MaxAlignment)

  template<typename... Args> explicit ImplValue(Args &&...args)
  {
    BLI_impl_value_type_check();
    new (buffer_.ptr()) T(std::forward<Args>(args)...);
  }

  ImplValue(const ImplValue &other)
  {
    BLI_impl_value_type_check();
    new (buffer_.ptr()) T(*other);
  }

  ImplValue(ImplValue &&other)
  {
    BLI_impl_value_type_check();
    new (buffer_.ptr()) T(std::move(*other));
  }

  ~ImplValue()
  {
    BLI_impl_value_type_check();
    std::destroy_at(reinterpret_cast<T *>(buffer_.ptr()));
  }

  ImplValue &operator=(const ImplValue &other)
  {
    BLI_impl_value_type_check();
    **this = *other;
    return *this;
  }

  ImplValue &operator=(ImplValue &&other)
  {
    BLI_impl_value_type_check();
    **this = std::move(*other);
    return *this;
  }

  T &operator*()
  {
    BLI_impl_value_type_check();
    return *reinterpret_cast<T *>(buffer_.ptr());
  }

  const T &operator*() const
  {
    BLI_impl_value_type_check();
    return *reinterpret_cast<const T *>(buffer_.ptr());
  }

  T *operator->()
  {
    BLI_impl_value_type_check();
    return reinterpret_cast<T *>(buffer_.ptr());
  }

  const T *operator->() const
  {
    BLI_impl_value_type_check();
    return reinterpret_cast<const T *>(buffer_.ptr());
  }

#undef BLI_impl_value_type_check
};

}  // namespace blender
