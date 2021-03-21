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

#include "BLI_virtual_array.hh"

#include "FN_generic_span.hh"

namespace blender::fn {

class GVArray {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVArray(const CPPType &type, const int64_t size) : type_(&type), size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~GVArray() = default;

  const CPPType &type() const
  {
    return *type_;
  }

  int64_t size() const
  {
    return size_;
  }

  bool is_empty() const
  {
    return size_;
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->get_impl(index, r_value);
  }

  void get_to_uninitialized(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->get_to_uninitialized_impl(index, r_value);
  }

  bool is_span() const
  {
    if (size_ == 0) {
      return true;
    }
    return this->is_span_impl();
  }

  GSpan get_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return GSpan(*type_);
    }
    return this->get_span_impl();
  }

  bool is_single() const
  {
    if (size_ == 1) {
      return true;
    }
    return this->is_single_impl();
  }

  void get_single(void *r_value) const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      this->get(0, r_value);
    }
    this->get_single_impl(r_value);
  }

  void get_single_to_uninitialized(void *r_value) const
  {
    type_->construct_default(r_value);
    this->get_single(r_value);
  }

  void materialize_to_uninitialized(const IndexMask mask, void *dst) const
  {
    for (const int64_t i : mask) {
      void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
      this->get_to_uninitialized(i, elem_dst);
    }
  }

 protected:
  virtual void get_impl(const int64_t index, void *r_value) const
  {
    type_->destruct(r_value);
    this->get_to_uninitialized_impl(index, r_value);
  }

  virtual void get_to_uninitialized_impl(const int64_t index, void *r_value) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual GSpan get_span_impl() const
  {
    BLI_assert(false);
    return GSpan(*type_);
  }

  virtual bool is_single_impl() const
  {
    return false;
  }

  virtual void get_single_impl(void *UNUSED(r_value)) const
  {
    BLI_assert(false);
  }
};

class GVArrayForGSpan : public GVArray {
 protected:
  const void *data_;
  const int64_t element_size_;

 public:
  GVArrayForGSpan(const GSpan span)
      : GVArray(span.type(), span.size()), data_(span.data()), element_size_(span.type().size())
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override
  {
    type_->copy_to_initialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    type_->copy_to_uninitialized(POINTER_OFFSET(data_, element_size_ * index), r_value);
  }

  bool is_span_impl() const override
  {
    return true;
  }

  GSpan get_span_impl() const override
  {
    return GSpan(*type_, data_, size_);
  }
};

class GVArrayForEmpty : public GVArray {
 public:
  GVArrayForEmpty(const CPPType &type) : GVArray(type, 0)
  {
  }

 protected:
  void get_to_uninitialized_impl(const int64_t UNUSED(index), void *UNUSED(r_value)) const override
  {
    BLI_assert(false);
  }
};

class GVArrayForSingleValueRef : public GVArray {
 private:
  const void *value_;

 public:
  GVArrayForSingleValueRef(const CPPType &type, const int64_t size, const void *value)
      : GVArray(type, size), value_(value)
  {
  }

 protected:
  void get_impl(const int64_t UNUSED(index), void *r_value) const override
  {
    type_->copy_to_initialized(value_, r_value);
  }

  void get_to_uninitialized_impl(const int64_t UNUSED(index), void *r_value) const override
  {
    type_->copy_to_uninitialized(value_, r_value);
  }

  bool is_span_impl() const override
  {
    return size_ == 1;
  }

  GSpan get_span_impl() const override
  {
    return GSpan{*type_, value_, 1};
  }

  bool is_single_impl() const override
  {
    return true;
  }

  void get_single_impl(void *r_value) const override
  {
    type_->copy_to_initialized(value_, r_value);
  }
};

template<typename T> class GVArrayForVArray : public GVArray {
 private:
  const VArray<T> &array_;

 public:
  GVArrayForVArray(const VArray<T> &array)
      : GVArray(CPPType::get<T>(), array.size()), array_(array)
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = array_.get(index);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(array_.get(index));
  }

  bool is_span_impl() const override
  {
    return array_.is_span();
  }

  GSpan get_span_impl() const override
  {
    return GSpan(array_.get_span());
  }

  bool is_single_impl() const override
  {
    return array_.is_single();
  }

  void get_single_impl(void *r_value) const override
  {
    *(T *)r_value = array_.get_single();
  }
};

template<typename T> class VArrayForGVArray : public VArray<T> {
 private:
  const GVArray &array_;

 public:
  VArrayForGVArray(const GVArray &array) : VArray<T>(array.size()), array_(array)
  {
    BLI_assert(array_.type().is<T>());
  }

 protected:
  T get_impl(const int64_t index) const override
  {
    T value;
    array_.get(index, &value);
    return value;
  }

  bool is_span_impl() const override
  {
    return array_.is_span();
  }

  Span<T> get_span_impl() const override
  {
    return array_.get_span().typed<T>();
  }

  bool is_single_impl() const override
  {
    return array_.is_single();
  }

  T get_single_impl() const override
  {
    T value;
    array_.get_single(&value);
    return value;
  }
};

}  // namespace blender::fn
