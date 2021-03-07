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

class GVSpan {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVSpan(const CPPType &type, const int64_t size = 0) : type_(&type), size_(size)
  {
  }

  virtual ~GVSpan() = default;

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
    this->get_element_impl(index, r_value);
  }

  bool is_span() const
  {
    return this->is_span_impl();
  }

  GSpan get_referenced_span() const
  {
    BLI_assert(this->is_span());
    return this->get_referenced_span_impl();
  }

 protected:
  virtual void get_element_impl(const int index, void *r_value) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual GSpan get_referenced_span_impl() const
  {
    BLI_assert(false);
    return {*type_};
  }
};

class GVMutableSpan {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVMutableSpan(const CPPType &type, const int64_t size = 0) : type_(&type), size_(size)
  {
  }

  virtual ~GVMutableSpan() = default;

 protected:
  virtual void get_element_impl(const int64_t index, void *r_value) const = 0;
  virtual void set_element_by_copy_impl(const int64_t index, const void *value) const = 0;
  virtual void set_element_by_move_impl(const int64_t index, void *value) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual GMutableSpan get_referenced_span_impl() const
  {
    BLI_assert(false);
    return {*type_};
  }
};

class GVSpanForGSpan final : public GVSpan {
 private:
  const void *data_ = nullptr;

 public:
  GVSpanForGSpan(const CPPType &type) : GVSpan(type, 0)
  {
  }

  GVSpanForGSpan(const GSpan span) : GVSpan(span.type(), span.size()), data_(span.data())
  {
  }

  template<typename T>
  GVSpanForGSpan(const Span<T> span)
      : GVSpan(CPPType::get<T>(), span.size()), data_((const void *)span.data())
  {
  }

 private:
  void get_element_impl(const int index, void *r_value) const final
  {
    const void *elem = POINTER_OFFSET(data_, type_->size() * index);
    type_->copy_to_initialized(elem, r_value);
  }

  bool is_span_impl() const final
  {
    return true;
  }

  GSpan get_referenced_span_impl() const final
  {
    return GSpan(*type_, data_, size_);
  }
};

template<typename T> class GVSpanForSpan final : public GVSpan {
 private:
  const T *data_ = nullptr;

 public:
  GVSpanForSpan() : GVSpanForSpan(CPPType::get<T>(), 0)
  {
  }

  GVSpanForSpan(const Span<T> span) : GVSpan(CPPType::get<T>(), span.size()), data_(span.data())
  {
  }

 private:
  void get_element_impl(const int index, void *r_value) const final
  {
    *(T *)r_value = data_[index];
  }

  bool is_span_impl() const final
  {
    return true;
  }

  GSpan get_referenced_span_impl() const final
  {
    return GSpan(*type_, data_, size_);
  }
};

class GVMutableSpanForGSpan final : public GVMutableSpan {
 private:
  void *data_ = nullptr;

 public:
  GVMutableSpanForGSpan(const CPPType &type) : GVMutableSpan(type, 0)
  {
  }

  GVMutableSpanForGSpan(const GMutableSpan span)
      : GVMutableSpan(span.type(), span.size()), data_(span.data())
  {
  }

 private:
  void get_element_impl(const int64_t index, void *r_value) const final
  {
    const void *elem = POINTER_OFFSET(data_, index * type_->size());
    type_->copy_to_initialized(elem, r_value);
  }

  void set_element_by_copy_impl(const int64_t index, const void *value) const final
  {
    void *elem = POINTER_OFFSET(data_, index * type_->size());
    type_->copy_to_initialized(value, elem);
  }

  void set_element_by_move_impl(const int64_t index, void *value) const final
  {
    void *elem = POINTER_OFFSET(data_, index * type_->size());
    type_->move_to_initialized(value, elem);
  }

  bool is_span_impl() const final
  {
    return true;
  }

  GMutableSpan get_referenced_span_impl() const final
  {
    return GMutableSpan(*type_, data_, size_);
  }
};

template<typename T> class GVMutableSpanForSpan : public GVMutableSpan {
 private:
  T *data_ = nullptr;

 public:
  GVMutableSpanForSpan() : GVMutableSpan(CPPType::get<T>(), 0)
  {
  }

  GVMutableSpanForSpan(const MutableSpan<T> span)
      : GVMutableSpan(CPPType::get<T>(), span.size()), data_(span.data())
  {
  }

 private:
  void get_element_impl(const int64_t index, void *r_value) const final
  {
    *(T *)r_value = data_[index];
  }

  void set_element_by_copy_impl(const int64_t index, const void *value) const final
  {
    data_[index] = *(const T *)value;
  }

  void set_element_by_move_impl(const int64_t index, void *value) const final
  {
    data_[index] = std::move(*(T *)value);
  }

  bool is_span_impl() const
  {
    return true;
  }

  GMutableSpan get_referenced_span_impl() const
  {
    return GMutableSpan(*type_, data_, size_);
  }
};

}  // namespace blender::fn
