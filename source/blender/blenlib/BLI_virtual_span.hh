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

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"
#include "BLI_span.hh"

namespace blender {

template<typename T> struct VSpanCallbacks {
  T (*get_element)(const void *user_data, const int64_t index);
  void (*materialize_to_initialized)(const void *user_data,
                                     const MutableSpan<T> dst,
                                     const IndexMask mask);
};

template<typename T> class VSpan {
 private:
  int64_t size_;
  const void *user_data_;
  const VSpanCallbacks<T> *callbacks_;

  static const VSpanCallbacks<T> &get_default_callbacks()
  {
    static const VSpanCallbacks<T> callbacks = {nullptr, nullptr};
    return callbacks;
  }

  static VSpanCallbacks<T> get_span_callbacks_impl()
  {
    VSpanCallbacks<T> callbacks;
    callbacks.get_element = [](const void *user_data, const int64_t index) -> T {
      const T *data = (const T *)user_data;
      return data[index];
    };
    callbacks.materialize_to_initialized =
        [](const void *user_data, const MutableSpan<T> dst, const IndexMask mask) {
          const T *data = (const T *)user_data;
          for (const int64_t i : mask) {
            dst[i] = data[i];
          }
        };
    return callbacks;
  }

  static const VSpanCallbacks<T> &get_span_callbacks()
  {
    static const VSpanCallbacks<T> callbacks = get_span_callbacks_impl();
    return callbacks;
  }

 public:
  VSpan() : size_(0), user_data_(nullptr), callbacks_(&get_default_callbacks())
  {
  }

  VSpan(const int64_t size, const void *user_data, const VSpanCallbacks<T> &callbacks)
      : size_(size), user_data_(user_data), callbacks_(&callbacks)
  {
    BLI_assert(size_ >= 0);
    BLI_assert(size_ == 0 || callbacks_->get_element != nullptr);
  }

  VSpan(const Span<T> span)
      : size_(span.size()),
        user_data_((const void *)span.data()),
        callbacks_(*get_span_callbacks())
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

  T operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return callbacks_->get_element(user_data_, index);
  }

  bool is_span() const
  {
    return callbacks_ == &get_span_callbacks();
  }

  Span<T> get_referenced_span() const
  {
    BLI_assert(this->is_span());
    return Span<T>((const T *)user_data_, size_);
  }

  void materialize_to_initialized(const MutableSpan<T> dst)
  {
    BLI_assert(dst.size() == size_);
    this->materialize_to_initialized(dst, IndexMask(size_));
  }

  void materialize_to_initialized(const MutableSpan<T> dst, const IndexMask mask)
  {
    BLI_assert(dst.size() >= mask.min_array_size());
    callbacks_->materialize_to_initialized(user_data_, dst, mask);
  }
};

template<typename T> struct VMutableSpanCallbacks {
  T (*get_element)(const void *user_data, const int64_t index);
  void (*set_element_by_copy)(void *user_data, const int64_t index, const T &value);
  void (*set_element_by_move)(void *user_data, const int64_t index, T &&value);
  void (*materialize_to_initialized)(const void *user_data,
                                     const MutableSpan<T> dst,
                                     const IndexMask mask);
};

template<typename T> class VMutableSpan {
 private:
  int64_t size_;
  void *user_data_;
  const VMutableSpanCallbacks<T> *callbacks_;

  static VMutableSpanCallbacks<T> get_default_callbacks_impl()
  {
    VMutableSpanCallbacks<T> callbacks;
    callbacks.get_element = nullptr;
    callbacks.set_element_by_copy = nullptr;
    callbacks.set_element_by_move = nullptr;
    callbacks.materialize_to_initialized =
        [](const void *user_data, const MutableSpan<T> dst, const IndexMask mask) {
          BLI_assert(mask.size() == 0);
        };
    return callbacks;
  }

  static const VMutableSpanCallbacks<T> &get_default_callbacks()
  {
    static const VMutableSpanCallbacks<T> callbacks = get_default_callbacks_impl();
    return callbacks;
  }

  static VMutableSpanCallbacks<T> get_span_callbacks_impl()
  {
    VMutableSpanCallbacks<T> callbacks;
    callbacks.get_element = [](const void *user_data, const int64_t index) -> T {
      const T *data = (const T *)user_data;
      return data[index];
    };
    callbacks.set_element_by_copy = [](void *user_data, const int64_t index, const T &value) {
      T *data = (T *)user_data;
      data[index] = value;
    };
    callbacks.set_element_by_move = [](void *user_data, const int64_t index, T &&value) {
      T *data = (T *)user_data;
      data[index] = std::move(value);
    };
    callbacks.materialize_to_initialized =
        [](const void *user_data, const MutableSpan<T> dst, const IndexMask mask) {
          const T *data = (const T *)user_data;
          for (const int64_t i : mask) {
            dst[i] = data[i];
          }
        };
    return callbacks;
  }

  static const VMutableSpanCallbacks<T> &get_span_callbacks()
  {
    static const VMutableSpanCallbacks<T> callbacks = get_span_callbacks_impl();
    return callbacks;
  }

 public:
  VMutableSpan() : size_(0), user_data_(nullptr), callbacks_(&get_default_callbacks())
  {
  }

  VMutableSpan(const int64_t size,
               const void *user_data,
               const VMutableSpanCallbacks<T> &callbacks)
      : size_(size), user_data_(user_data), callbacks_(callbacks)
  {
    BLI_assert(size_ >= 0);
    BLI_assert(size_ == 0 || callbacks_->get_element != nullptr);
  }

  VMutableSpan(const MutableSpan<T> span)
      : size_(span.size()), user_data_((void *)span.data()), callbacks_(*get_span_callbacks())
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

  T operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return callbacks_->get_element(user_data_, index);
  }

  void set(const int64_t index, const T &value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    callbacks_->set_element_by_copy(user_data_, index, value);
  }

  void set(const int64_t index, T &&value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    callbacks_->set_element_by_move(user_data_, index, std::move(value));
  }

  bool is_span() const
  {
    return callbacks_ == &get_span_callbacks();
  }

  MutableSpan<T> get_referenced_span() const
  {
    BLI_assert(this->is_span());
    return MutableSpan<T>((T *)user_data_, size_);
  }

  void materialize_to_initialized(const MutableSpan<T> dst)
  {
    BLI_assert(dst.size() == size_);
    this->materialize_to_initialized(dst, IndexMask(size_));
  }

  void materialize_to_initialized(const MutableSpan<T> dst, const IndexMask mask)
  {
    BLI_assert(dst.size() >= mask.min_array_size());
    callbacks_->materialize_to_initialized(user_data_, dst, mask);
  }
};

}  // namespace blender
