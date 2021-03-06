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

struct GVSpanCallbacks {
  void (*get_element)(const void *user_data,
                      const CPPType &type,
                      const int64_t index,
                      void *r_value);
};

class GVSpan {
 private:
  int64_t size_;
  const void *user_data_;
  const GVSpanCallbacks *callbacks_;
  const CPPType *type_;

  static const GVSpanCallbacks &get_default_callbacks()
  {
    static const GVSpanCallbacks callbacks = {nullptr};
    return callbacks;
  }

  static GVSpanCallbacks get_gspan_callbacks_impl()
  {
    GVSpanCallbacks callbacks;
    callbacks.get_element =
        [](const void *user_data, const CPPType &type, const int64_t index, void *r_value) {
          const void *elem = POINTER_OFFSET(user_data, index * type.size());
          type.copy_to_initialized(elem, r_value);
        };
    return callbacks;
  }

  static const GVSpanCallbacks &get_gspan_callbacks()
  {
    static const GVSpanCallbacks callbacks = get_gspan_callbacks_impl();
    return callbacks;
  }

 public:
  GVSpan() : size_(0), user_data_(nullptr), callbacks_(&get_default_callbacks()), type_(nullptr)
  {
  }

  GVSpan(const int64_t size,
         const void *user_data,
         const GVSpanCallbacks &callbacks,
         const CPPType &type)
      : size_(size), user_data_(user_data), callbacks_(&callbacks), type_(&type)
  {
  }

  template<typename T>
  GVSpan(const Span<T> span)
      : size_(span.size()),
        user_data_((const void *)span.data()),
        callbacks_(&get_gspan_callbacks()),
        type_(CPPType::get<T>())
  {
  }

  GVSpan(const GSpan span)
      : size_(span.size()),
        user_data_(span.data()),
        callbacks_(&get_gspan_callbacks()),
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
    callbacks_->get_element(user_data_, *type_, index, r_value);
  }

  bool is_span() const
  {
    return callbacks_ == &get_gspan_callbacks();
  }

  GSpan get_referenced_span() const
  {
    BLI_assert(this->is_span());
    return GSpan(*type_, user_data_, size_);
  }
};

struct GVMutableSpanCallbacks {
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

class GVMutableSpan {
 private:
  int64_t size_;
  const void *user_data_;
  const GVMutableSpanCallbacks *callbacks_;
  const CPPType *type_;
};

}  // namespace blender::fn
