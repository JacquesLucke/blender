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
 * \ingroup fn
 */

#include "BLI_virtual_span.hh"

#include "FN_generic_span.hh"

namespace blender::fn {

struct GVSpanVTable {
  void (*get_element)(const void *user_data,
                      const CPPType &type,
                      const int64_t index,
                      void *r_value) = nullptr;
  bool is_span = false;
};

struct GVMutableSpanVTable {
  void (*get_element)(const void *user_data,
                      const CPPType &type,
                      const int64_t index,
                      void *r_value);
  void (*set_element_by_copy)(void *user_data,
                              const CPPType &type,
                              const int64_t index,
                              const void *value);
  void (*set_element_by_move)(void *user_data,
                              const CPPType &type,
                              const int64_t index,
                              void *value);
};

class GVSpan {
 private:
  int64_t size_;
  const void *user_data_;
  const GVSpanVTable *vtable;
  const CPPType *type_;

 public:
  GVSpan() : size_(0), user_data_(nullptr), vtable(&get_default_vtable()), type_(nullptr)
  {
  }

  GVSpan(const int64_t size,
         const void *user_data,
         const GVSpanVTable &vtable,
         const CPPType &type)
      : size_(size), user_data_(user_data), vtable(&vtable), type_(&type)
  {
  }

  template<typename T>
  GVSpan(const Span<T> span)
      : size_(span.size()),
        user_data_((const void *)span.data()),
        vtable(&get_span_vtable<T>()),
        type_(CPPType::get<T>())
  {
  }

  GVSpan(const GSpan span)
      : size_(span.size()),
        user_data_(span.data()),
        vtable(&get_gspan_vtable()),
        type_(&span.type())
  {
  }

  int64_t size() const
  {
    return size_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    vtable->get_element(user_data_, *type_, index, r_value);
  }

  bool is_span() const
  {
    return vtable->is_span;
  }

  GSpan get_referenced_span() const
  {
    BLI_assert(this->is_span());
    return GSpan(*type_, user_data_, size_);
  }

 private:
  static const GVSpanVTable &get_default_vtable()
  {
    static const GVSpanVTable vtable = {nullptr};
    return vtable;
  }

  static GVSpanVTable get_gspan_vtableimpl()
  {
    GVSpanVTable vtable;
    vtable.is_span = true;
    vtable.get_element =
        [](const void *user_data, const CPPType &type, const int64_t index, void *r_value) {
          const void *elem = POINTER_OFFSET(user_data, index * type.size());
          type.copy_to_initialized(elem, r_value);
        };
    return vtable;
  }

  static const GVSpanVTable &get_gspan_vtable()
  {
    static const GVSpanVTable vtable = get_gspan_vtableimpl();
    return vtable;
  }

  template<typename T> static GVSpanVTable get_span_vtableimpl()
  {
    GVSpanVTable vtable;
    vtable.is_span = true;
    vtable.get_element =
        [](const void *user_data, const CPPType &type, const int64_t index, void *r_value) {
          const T *data = (const T *)user_data;
          *(T *)r_value = data[index];
        };
    return vtable;
  }

  template<typename T> static const GVSpanVTable &get_span_vtable()
  {
    static const GVSpanVTable vtable = get_span_vtableimpl<T>();
    return vtable;
  }
};

class GVMutableSpan {
 private:
  int64_t size_;
  const void *user_data_;
  const GVMutableSpanVTable *vtable;
  const CPPType *type_;
};

}  // namespace blender::fn
