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

template<typename T> class VSpan {
 protected:
  int64_t size_ = 0;

 public:
  VSpan(const int64_t size = 0) : size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~VSpan() = default;

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
    return this->get_element_impl(index);
  }

  bool is_span() const
  {
    if (size_ == 0) {
      return true;
    }
    return this->is_span_impl();
  }

  Span<T> get_referenced_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return {};
    }
    return this->get_referenced_span_impl();
  }

 protected:
  virtual T get_element_impl(const int64_t index) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual Span<T> get_referenced_span_impl() const
  {
    BLI_assert(false);
    return {};
  }
};

template<typename T> class VMutableSpan {
 protected:
  int64_t size_;

 public:
  VMutableSpan(int64_t size = 0) : size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~VMutableSpan() = default;

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
    return this->get_element(index);
  }

  void set(const int64_t index, const T &value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_element_by_copy(index, value);
  }

  void set(const int64_t index, T &&value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_element_by_move(index, std::move(value));
  }

  bool is_span() const
  {
    if (size_ == 0) {
      return true;
    }
    return this->is_span_impl();
  }

  MutableSpan<T> get_referenced_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return {};
    }
    return this->get_referenced_span_impl();
  }

 protected:
  virtual T get_element_impl(const int64_t index) const = 0;

  virtual void set_element_by_copy_impl(const int64_t index, const T &value) const
  {
    this->set_element_by_move_impl(index, T(value));
  }

  virtual void set_element_by_move_impl(const int64_t index, T &&value) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual MutableSpan<T> get_referenced_span_impl() const
  {
    BLI_assert(false);
    return {};
  }
};

template<typename T> class VSpanForSpan final : public VSpan<T> {
 private:
  const T *data_ = nullptr;

 public:
  VSpanForSpan() = default;

  VSpanForSpan(const Span<T> span) : VSpan<T>(span.size()), data_(span.data())
  {
  }

 protected:
  T get_element_impl(const int64_t index) const final
  {
    return data_[index];
  }

  bool is_span_impl() const final
  {
    return true;
  }

  Span<T> get_referenced_span_impl() const final
  {
    return Span<T>(data_, this->size_);
  }
};

template<typename T> class VMutableSpanForSpan final : public VMutableSpan<T> {
 private:
  T *data_ = nullptr;

 public:
  VMutableSpanForSpan() = default;

  VMutableSpanForSpan(const MutableSpan<T> span) : VMutableSpan<T>(span.size()), data_(span.data())
  {
  }

 protected:
  T get_element_impl(const int64_t index) const final
  {
    return data_[index];
  }

  void set_element_by_copy_impl(const int64_t index, const T &value) const final
  {
    data_[index] = value;
  }

  void set_element_by_move_impl(const int64_t index, T &&value) const final
  {
    data_[index] = std::move(value);
  }

  bool is_span_impl() const final
  {
    return true;
  }

  MutableSpan<T> get_referenced_span_impl() const final
  {
    return MutableSpan<T>(data_, this->size_);
  }
};

}  // namespace blender
