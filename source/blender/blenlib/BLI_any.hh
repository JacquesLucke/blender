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

#pragma once

/** \file
 * \ingroup bli
 */

#include <utility>

#include "BLI_memory_utils.hh"

namespace blender {

namespace detail {
struct AnyTypeInfo {
  bool is_unique_ptr;
  void (*copy_construct)(void *dst, const void *src);
  void (*move_construct)(void *dst, void *src);
  void (*destruct)(void *src);
  const void *(*get)(const void *src);

  template<typename T> static const AnyTypeInfo &get_for_inline()
  {
    static AnyTypeInfo funcs = {false,
                                [](void *dst, const void *src) { new (dst) T(*(const T *)src); },
                                [](void *dst, void *src) { new (dst) T(std::move(*(T *)src)); },
                                [](void *src) { ((T *)src)->~T(); },
                                [](const void *src) { return src; }};
    return funcs;
  }

  template<typename T> static const AnyTypeInfo &get_for_unique_ptr()
  {
    using Ptr = std::unique_ptr<T>;
    static AnyTypeInfo funcs = {
        true,
        [](void *dst, const void *src) { new (dst) Ptr(new T(**(const Ptr *)src)); },
        [](void *dst, void *src) { new (dst) Ptr(new T(std::move(**(Ptr *)src))); },
        [](void *src) { ((Ptr *)src)->~Ptr(); },
        [](const void *src) -> const void * { return &**(const Ptr *)src; }};
    return funcs;
  }

  static const AnyTypeInfo &get_for_empty()
  {
    static AnyTypeInfo funcs = {false,
                                [](void *UNUSED(dst), const void *UNUSED(src)) {},
                                [](void *UNUSED(dst), void *UNUSED(src)) {},
                                [](void *UNUSED(src)) {},
                                [](const void *UNUSED(src)) -> const void * { return nullptr; }};
    return funcs;
  }
};
}  // namespace detail

template<size_t InlineBufferCapacity = sizeof(void *), size_t Alignment = alignof(void *)>
class Any {
 private:
  using Info = detail::AnyTypeInfo;

  AlignedBuffer<std::max(InlineBufferCapacity, sizeof(std::unique_ptr<int>)), Alignment> buffer_;
  const Info *info_ = &Info::get_for_empty();

 public:
  template<typename T> static constexpr inline bool is_allowed_v = std::is_copy_constructible_v<T>;

  template<typename T>
  static constexpr inline bool is_inline_v = std::is_nothrow_move_constructible_v<T> &&
                                             sizeof(T) <= InlineBufferCapacity &&
                                             alignof(T) <= Alignment;

  template<typename T>
  static constexpr inline bool is_same_any_v = std::is_same_v<std::decay_t<T>, Any>;

 private:
  template<typename T> const Info &get_info() const
  {
    using DecayT = std::decay_t<T>;
    static_assert(is_allowed_v<DecayT>);
    if constexpr (is_inline_v<DecayT>) {
      return Info::get_for_inline<DecayT>();
    }
    else {
      return Info::get_for_unique_ptr<DecayT>();
    }
  }

 public:
  Any() = default;

  Any(const Any &other)
  {
    info_ = other.info_;
    info_->copy_construct(&buffer_, &other.buffer_);
  }

  Any(Any &&other)
  {
    info_ = other.info_;
    info_->move_construct(&buffer_, &other.buffer_);
  }

  template<typename T, typename... Args> explicit Any(std::in_place_type_t<T>, Args &&...args)
  {
    using DecayT = std::decay_t<T>;
    static_assert(is_allowed_v<DecayT>);
    info_ = &this->template get_info<DecayT>();
    if constexpr (is_inline_v<DecayT>) {
      new (&buffer_) T(std::forward<Args>(args)...);
    }
    else {
      new (&buffer_) std::unique_ptr<DecayT>(new DecayT(std::forward<Args>(args)...));
    }
  }

  template<typename T, typename X = std::enable_if_t<!is_same_any_v<T>, void>>
  Any(T &&value) : Any(std::in_place_type<T>, std::forward<T>(value))
  {
  }

  ~Any()
  {
    info_->destruct(&buffer_);
  }

  template<typename T> Any &operator=(T &&other)
  {
    if constexpr (is_same_any_v<T>) {
      if (this == &other) {
        return *this;
      }
    }
    this->~Any();
    new (this) Any(std::forward<T>(other));
    return *this;
  }

  void reset()
  {
    info_->destruct(&buffer_);
    info_ = &Info::get_for_empty();
  }

  bool is_empty() const
  {
    return info_ == &Info::get_for_empty();
  }

  template<typename T> bool is() const
  {
    return info_ == &this->template get_info<T>();
  }

  void *get()
  {
    return const_cast<void *>(info_->get(&buffer_));
  }

  const void *get() const
  {
    return info_->get(&buffer_);
  }

  template<typename T> T &get()
  {
    BLI_assert(this->is<T>());
    return *static_cast<T *>(this->get());
  }

  template<typename T> const T &get() const
  {
    BLI_assert(this->is<T>());
    return *static_cast<const T *>(this->get());
  }
};

}  // namespace blender
