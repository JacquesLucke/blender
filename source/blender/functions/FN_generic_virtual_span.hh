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
  bool is_span = false;
  void (*get_element)(const void *user_data,
                      const CPPType &type,
                      const int64_t index,
                      void *r_value) = nullptr;
};

struct GVMutableSpanVTable {
  bool is_span = false;
  void (*get_element)(const void *user_data,
                      const CPPType &type,
                      const int64_t index,
                      void *r_value) = nullptr;
  void (*set_element_by_copy)(void *user_data,
                              const CPPType &type,
                              const int64_t index,
                              const void *value) = nullptr;
  void (*set_element_by_move)(void *user_data,
                              const CPPType &type,
                              const int64_t index,
                              void *value) = nullptr;
};

class GVMutableSpan;

class GVSpan {
 private:
  int64_t size_;
  const void *user_data_;
  const GVSpanVTable *vtable_;
  const CPPType *type_;

  friend GVMutableSpan;

 public:
  GVSpan() : size_(0), user_data_(nullptr), vtable_(&get_default_vtable()), type_(nullptr)
  {
  }

  GVSpan(const int64_t size,
         const void *user_data,
         const GVSpanVTable &vtable,
         const CPPType &type)
      : size_(size), user_data_(user_data), vtable_(&vtable), type_(&type)
  {
  }

  template<typename T>
  GVSpan(const Span<T> span)
      : size_(span.size()),
        user_data_((const void *)span.data()),
        vtable_(&get_span_vtable<T>()),
        type_(CPPType::get<T>())
  {
  }

  GVSpan(const GSpan span)
      : size_(span.size()),
        user_data_(span.data()),
        vtable_(&get_gspan_vtable()),
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

  const CPPType &type() const
  {
    return *type_;
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    vtable_->get_element(user_data_, *type_, index, r_value);
  }

  bool is_span() const
  {
    return vtable_->is_span;
  }

  GSpan get_referenced_span() const
  {
    BLI_assert(this->is_span());
    return GSpan(*type_, user_data_, size_);
  }

 private:
  static const GVSpanVTable &get_default_vtable()
  {
    static const GVSpanVTable vtable = {false, nullptr};
    return vtable;
  }

  static GVSpanVTable get_gspan_vtable_impl()
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
    static const GVSpanVTable vtable = get_gspan_vtable_impl();
    return vtable;
  }

  template<typename T> static GVSpanVTable get_span_vtable_impl()
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
    static const GVSpanVTable vtable = get_span_vtable_impl<T>();
    return vtable;
  }
};

class GVMutableSpan {
 private:
  int64_t size_;
  void *user_data_;
  const GVMutableSpanVTable *vtable_;
  const CPPType *type_;

 public:
  GVMutableSpan() : size_(0), user_data_(0), vtable_(&get_default_vtable()), type_(nullptr)
  {
  }

  GVMutableSpan(const int64_t size,
                void *user_data,
                const GVMutableSpanVTable &vtable,
                const CPPType &type)
      : size_(size), user_data_(user_data), vtable_(&vtable), type_(&type)
  {
  }

  template<typename T>
  GVMutableSpan(const MutableSpan<T> span)
      : size_(span.size()),
        user_data_(span.data()),
        vtable_(&get_span_vtable<T>()),
        type_(&CPPType::get<T>())
  {
  }

  GVMutableSpan(const GMutableSpan span)
      : size_(span.size()),
        user_data_(span.data()),
        vtable_(&get_gspan_vtable()),
        type_(&span.type())
  {
  }

 private:
  static const GVMutableSpanVTable &get_default_vtable()
  {
    static const GVMutableSpanVTable vtable = {false, nullptr, nullptr, nullptr};
    return vtable;
  }

  static GVMutableSpanVTable get_gspan_vtable_impl()
  {
    const GVSpanVTable &non_mutable_vtable = GVSpan::get_gspan_vtable();
    GVMutableSpanVTable vtable;
    vtable.is_span = true;
    vtable.get_element = non_mutable_vtable.get_element;
    vtable.set_element_by_copy =
        [](void *user_data, const CPPType &type, const int64_t index, const void *value) {
          void *elem = POINTER_OFFSET(user_data, index * type.size());
          type.copy_to_initialized(value, elem);
        };
    vtable.set_element_by_move =
        [](void *user_data, const CPPType &type, const int64_t index, void *value) {
          void *elem = POINTER_OFFSET(user_data, index * type.size());
          type.move_to_initialized(value, elem);
        };
    return vtable;
  }

  static const GVMutableSpanVTable &get_gspan_vtable()
  {
    static const GVMutableSpanVTable vtable = get_gspan_vtable_impl();
    return vtable;
  }

  template<typename T> GVMutableSpanVTable get_span_vtable_impl()
  {
    const GVMutableSpanVTable &non_mutable_vtable = GVSpan::get_span_vtable<T>();
    GVMutableSpanVTable vtable;
    vtable.is_span = true;
    vtable.get_element = non_mutable_vtable.get_element;
    vtable.set_element_by_copy =
        [](void *user_data, const CPPType &type, const int64_t index, const void *value) {
          T *data = (T *)user_data;
          const T &typed_value = *(const T *)value;
          data[index] = typed_value;
        };
    vtable.set_element_by_move =
        [](void *user_data, const CPPType &type, const int64_t index, void *value) {
          T *data = (T *)user_data;
          T &typed_value = *(T *)value;
          data[index] = std::move(typed_value);
        };
    return vtable;
  }

  template<typename T> const GVMutableSpanVTable &get_span_vtable()
  {
    static const GVMutableSpanVTable vtable = get_span_vtable_impl<T>();
    return vtable;
  }
};

}  // namespace blender::fn
