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

#include "BLI_virtual_array_span.hh"

#include "FN_generic_virtual_span.hh"

namespace blender::fn {

class GVArraySpan {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVArraySpan(const CPPType &type, const int64_t size) : type_(&type), size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~GVArraySpan() = default;

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

  int64_t get_array_size(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_array_size_impl(index);
  }

  void get_array_element(const int64_t index, const int64_t index_in_array, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    BLI_assert(index_in_array >= 0);
    BLI_assert(index_in_array < this->get_array_size(index));
    return this->get_array_element_impl(index, index_in_array, r_value);
  }

  bool is_single_array() const
  {
    return this->is_single_array_impl();
  }

 private:
  virtual int64_t get_array_size_impl(const int64_t index) const = 0;
  virtual void get_array_element_impl(const int64_t index,
                                      const int64_t index_in_array,
                                      void *r_value) const = 0;

  virtual bool is_single_array_impl() const
  {
    return false;
  }
};

class GVArraySpanForSingleGSpan final : public GVArraySpan {
 private:
  const void *data_;
  const int64_t array_size_;

 public:
  GVArraySpanForSingleGSpan(const GSpan span, const int64_t size)
      : GVArraySpan(span.type(), size), data_(span.data()), array_size_(span.size())
  {
  }

 private:
  int64_t get_array_size_impl(const int64_t UNUSED(index)) const final
  {
    return array_size_;
  }

  void get_array_element_impl(const int64_t UNUSED(index),
                              const int64_t index_in_array,
                              void *r_value) const final
  {
    const void *elem = POINTER_OFFSET(data_, type_->size() * index_in_array);
    type_->copy_to_initialized(elem, r_value);
  }

  bool is_single_array_impl() const final
  {
    return true;
  }
};

class GVArraySpanForStartsAndSizes final : public GVArraySpan {
 private:
  const void *const *starts_;
  const int64_t *sizes_;

 public:
  GVArraySpanForStartsAndSizes(const CPPType &type,
                               const Span<const void *> starts,
                               const Span<int64_t> sizes)
      : GVArraySpan(type, starts.size()), starts_(starts.data()), sizes_(sizes.data())
  {
  }

 public:
  int64_t get_array_size_impl(const int64_t index) const final
  {
    return sizes_[index];
  }

  void get_array_element_impl(const int64_t index,
                              const int64_t index_in_array,
                              void *r_value) const final
  {
    const void *elem = POINTER_OFFSET(starts_[index], type_->size() * index_in_array);
    type_->copy_to_initialized(elem, r_value);
  }
};

template<typename T> class VArraySpanForGVArraySpan : public VArraySpan<T> {
 private:
  const GVArraySpan &array_span_;

 public:
  VArraySpanForGVArraySpan(const GVArraySpan &array_span)
      : VArraySpan<T>(array_span.size()), array_span_(array_span)
  {
  }

 private:
  int64_t get_array_size_impl(const int64_t index) const final
  {
    return array_span_.get_array_size(index);
  }

  T get_array_element_impl(const int64_t index, const int64_t index_in_array) const final
  {
    T value;
    array_span_.get_array_element(index, index_in_array, &value);
    return value;
  }

  bool is_single_array_impl() const final
  {
    return array_span_.is_single_array();
  }
};

class GVSpanForGVArraySpan final : public GVSpan {
 private:
  const GVArraySpan &array_span_;
  const int64_t index_;

 public:
  GVSpanForGVArraySpan(const GVArraySpan &array_span, const int64_t index)
      : GVSpan(array_span.type(), array_span.get_array_size(index)),
        array_span_(array_span),
        index_(index)
  {
  }

 private:
  void get_element_impl(const int64_t index_in_array, void *r_value) const final
  {
    array_span_.get_array_element(index_, index_in_array, r_value);
  }
};

}  // namespace blender::fn
