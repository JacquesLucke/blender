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

#include "BLI_memory_utils.hh"

namespace blender {

namespace detail {
struct AnyTypeInfo {
  bool is_unique_ptr;
  void (*copy_construct)(void *dst, const void *src);
  void (*move_construct)(void *dst, void *src);
  void (*copy_assign)(void *dst, const void *src);
  void (*move_assign)(void *dst, void *src);
  void (*destruct)(void *src);
  const void *(*get)(const void *src);

  template<typename T> static const AnyTypeInfo &get_for_inline()
  {
    static AnyTypeInfo funcs = {false,
                                [](void *dst, const void *src) { new (dst) T(*(const T *)src); },
                                [](void *dst, void *src) { new (dst) T(std::move(*(T *)src)); },
                                [](void *dst, const void *src) { *(T *)dst = *(const T *)src; },
                                [](void *dst, void *src) { *(T *)dst = std::move(*(T *)src); },
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
        [](void *dst, const void *src) { *(Ptr *)dst = Ptr(new T(**(const Ptr *)src)); },
        [](void *dst, void *src) { *(Ptr *)dst = Ptr(new T(std::move(**(Ptr *)src))); },
        [](void *src) { ((Ptr *)src)->~Ptr(); },
        [](const void *src) -> const void * { return &**(const Ptr *)src; }};
    return funcs;
  }

  static const AnyTypeInfo &get_for_empty()
  {
    static AnyTypeInfo funcs = {false,
                                [](void *UNUSED(dst), const void *UNUSED(src)) {},
                                [](void *UNUSED(dst), void *UNUSED(src)) {},
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

  template<typename T> static constexpr inline bool is_allowed_v = std::is_copy_constructible_v<T>;

  template<typename T>
  static constexpr inline bool is_inline_v = std::is_nothrow_move_constructible_v<T> &&
                                             sizeof(T) <= InlineBufferCapacity &&
                                             alignof(T) <= Alignment;

 public:
  Any() = default;
};

}  // namespace blender
