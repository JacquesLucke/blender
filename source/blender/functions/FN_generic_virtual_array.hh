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
 *
 * A generic virtual array is the same as a virtual array from blenlib, except for the fact that
 * the data type is only known at runtime.
 */

#include "BLI_virtual_array.hh"

#include "FN_generic_span.hh"

namespace blender::fn {

/* A generically typed version of `VArray<T>`. */
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

  /* Copies the value at the given index into the provided storage. The `r_value` pointer is
   * expected to point to initialized memory. */
  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->get_impl(index, r_value);
  }

  /* Same as `get`, but `r_value` is expected to point to uninitialized memory. */
  void get_to_uninitialized(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->get_to_uninitialized_impl(index, r_value);
  }

  /* Returns true when the virtual array is stored as a span internally. */
  bool is_span() const
  {
    if (size_ == 0) {
      return true;
    }
    return this->is_span_impl();
  }

  /* Returns the internally used span of the virtual array. This invokes undefined behavior is the
   * virtual array is not stored as a span internally. */
  GSpan get_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return GSpan(*type_);
    }
    return this->get_span_impl();
  }

  /* Returns true when the virtual array returns the same value for every index. */
  bool is_single() const
  {
    if (size_ == 1) {
      return true;
    }
    return this->is_single_impl();
  }

  /* Copies the value that is used for every element into `r_value`, which is expected to point to
   * initialized memory. This invokes undefined behavior if the virtual array would not return the
   * same value for every index. */
  void get_single(void *r_value) const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      this->get(0, r_value);
    }
    this->get_single_impl(r_value);
  }

  /* Same as `get_single`, but `r_value` points to initialized memory. */
  void get_single_to_uninitialized(void *r_value) const
  {
    type_->construct_default(r_value);
    this->get_single(r_value);
  }

  void materialize_to_uninitialized(const IndexMask mask, void *dst) const;

 protected:
  virtual void get_impl(const int64_t index, void *r_value) const;
  virtual void get_to_uninitialized_impl(const int64_t index, void *r_value) const = 0;

  virtual bool is_span_impl() const;
  virtual GSpan get_span_impl() const;

  virtual bool is_single_impl() const;
  virtual void get_single_impl(void *UNUSED(r_value)) const;
};

class GVMutableArray : public GVArray {
 public:
  GVMutableArray(const CPPType &type, const int64_t size) : GVArray(type, size)
  {
  }

  void set_by_copy(const int64_t index, const void *value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_by_copy_impl(index, value);
  }

  void set_by_move(const int64_t index, void *value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_by_move_impl(index, value);
  }

  void set_by_relocate(const int64_t index, void *value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_by_relocate_impl(index, value);
  }

  GMutableSpan get_span()
  {
    BLI_assert(this->is_span());
    GSpan span = static_cast<const GVArray *>(this)->get_span();
    return GMutableSpan(span.type(), const_cast<void *>(span.data()), span.size());
  }

 protected:
  virtual void set_by_copy_impl(const int64_t index, const void *value)
  {
    BUFFER_FOR_CPP_TYPE_VALUE(*type_, buffer);
    type_->copy_to_uninitialized(value, buffer);
    this->set_by_move_impl(index, buffer);
    type_->destruct(buffer);
  }

  virtual void set_by_relocate_impl(const int64_t index, void *value)
  {
    this->set_by_move_impl(index, value);
    type_->destruct(value);
  }

  virtual void set_by_move_impl(const int64_t index, void *value) = 0;
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
  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  bool is_span_impl() const override;
  GSpan get_span_impl() const override;
};

class GVMutableArrayForGMutableSpan : public GVMutableArray {
 protected:
  void *data_;
  const int64_t element_size_;

 public:
  GVMutableArrayForGMutableSpan(const GMutableSpan span)
      : GVMutableArray(span.type(), span.size()),
        data_(span.data()),
        element_size_(span.type().size())
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  void set_by_copy_impl(const int64_t index, const void *value) override;
  void set_by_move_impl(const int64_t index, void *value) override;
  void set_by_relocate_impl(const int64_t index, void *value) override;

  bool is_span_impl() const override;
  GSpan get_span_impl() const override;
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
  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  bool is_span_impl() const override;
  GSpan get_span_impl() const override;

  bool is_single_impl() const override;
  void get_single_impl(void *r_value) const override;
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
    BLI_assert(array_.type().template is<T>());
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
    return array_.get_span().template typed<T>();
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

template<typename T> class GVArrayForSpan : public GVArray {
 private:
  const T *data_;

 public:
  GVArrayForSpan(const Span<T> data) : GVArray(CPPType::get<T>(), data.size()), data_(data.data())
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = data_[index];
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  bool is_span_impl() const
  {
    return true;
  }

  GSpan get_span_impl() const
  {
    return GSpan(*type_, data_, size_);
  }
};

template<typename T> class GVMutableArrayForMutableSpan : public GVMutableArray {
 private:
  T *data_;

 public:
  GVMutableArrayForMutableSpan(const MutableSpan<T> data)
      : GVMutableArray(CPPType::get<T>(), data.size()), data_(data.data())
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = data_[index];
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void set_by_copy_impl(const int64_t index, const void *value) override
  {
    data_[index] = *(const T *)value;
  }

  void set_by_relocate(const int64_t index, void *value) override
  {
    data_[index] = std::move(*(T *)value);
    ((T *)value)->~T();
  }

  void set_by_move(const int64_t index, void *value) override
  {
    data_[index] = std::move(*(T *)value);
  }

  bool is_span_impl() const
  {
    return true;
  }

  GSpan get_span_impl() const
  {
    return GSpan(*type_, data_, size_);
  }
};

template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class GVArrayForDerivedSpan : public GVArray {
 private:
  const StructT *data_;

 public:
  GVArrayForDerivedSpan(Span<StructT> data)
      : GVArray(CPPType::get<ElemT>(), data.size()), data_(data.data())
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override
  {
    *(ElemT *)r_value = GetFunc(data_[index]);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) ElemT(GetFunc(data_[index]));
  }
};

template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, ElemT)>
class GVMutableArrayForDerivedSpan : public GVMutableArray {
 private:
  StructT *data_;

 public:
  GVMutableArrayForDerivedSpan(MutableSpan<StructT> data)
      : GVMutableArray(CPPType::get<ElemT>(), data.size()), data_(data.data())
  {
  }

 protected:
  void get_impl(const int64_t index, void *r_value) const override
  {
    *(ElemT *)r_value = GetFunc(data_[index]);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) ElemT(GetFunc(data_[index]));
  }

  void set_by_copy_impl(const int64_t index, const void *value) override
  {
    SetFunc(data_[index], *(const ElemT *)value);
  }

  void set_by_relocate_impl(const int64_t index, void *value) override
  {
    SetFunc(data_[index], std::move(*(ElemT *)value));
    ((ElemT *)value)->~ElemT();
  }

  void set_by_move_impl(const int64_t index, void *value) override
  {
    SetFunc(data_[index], std::move(*(ElemT *)value));
  }
};

}  // namespace blender::fn
