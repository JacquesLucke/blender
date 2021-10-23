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

#include <optional>

#include "BLI_virtual_array.hh"

#include "FN_generic_array.hh"
#include "FN_generic_span.hh"

namespace blender::fn {

template<typename T> class GVArray_Typed;
template<typename T> class GVMutableArray_Typed;

class GVArrayImpl;
class GVMutableArrayImpl;

using GVArrayPtr = std::unique_ptr<GVArrayImpl>;
using GVMutableArrayPtr = std::unique_ptr<GVMutableArrayImpl>;

/* A generically typed version of `VArrayImpl<T>`. */
class GVArrayImpl {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVArrayImpl(const CPPType &type, const int64_t size) : type_(&type), size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~GVArrayImpl() = default;

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
    return size_ == 0;
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
  GSpan get_internal_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return GSpan(*type_);
    }
    return this->get_internal_span_impl();
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
  void get_internal_single(void *r_value) const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      this->get(0, r_value);
      return;
    }
    this->get_internal_single_impl(r_value);
  }

  /* Same as `get_internal_single`, but `r_value` points to initialized memory. */
  void get_internal_single_to_uninitialized(void *r_value) const
  {
    type_->default_construct(r_value);
    this->get_internal_single(r_value);
  }

  void materialize(void *dst) const;
  void materialize(const IndexMask mask, void *dst) const;

  void materialize_to_uninitialized(void *dst) const;
  void materialize_to_uninitialized(const IndexMask mask, void *dst) const;

  template<typename T> const VArrayImpl<T> *try_get_internal_varray() const
  {
    BLI_assert(type_->is<T>());
    return (const VArrayImpl<T> *)this->try_get_internal_varray_impl();
  }

  /* Create a typed virtual array for this generic virtual array. */
  template<typename T> GVArray_Typed<T> typed() const
  {
    return GVArray_Typed<T>(*this);
  }

  GVArrayPtr shallow_copy() const;

 protected:
  virtual void get_impl(const int64_t index, void *r_value) const;
  virtual void get_to_uninitialized_impl(const int64_t index, void *r_value) const = 0;

  virtual bool is_span_impl() const;
  virtual GSpan get_internal_span_impl() const;

  virtual bool is_single_impl() const;
  virtual void get_internal_single_impl(void *UNUSED(r_value)) const;

  virtual void materialize_impl(const IndexMask mask, void *dst) const;
  virtual void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const;

  virtual const void *try_get_internal_varray_impl() const;
};

/* Similar to GVArrayImpl, but supports changing the elements in the virtual array. */
class GVMutableArrayImpl : public GVArrayImpl {
 public:
  GVMutableArrayImpl(const CPPType &type, const int64_t size) : GVArrayImpl(type, size)
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

  GMutableSpan get_internal_span()
  {
    BLI_assert(this->is_span());
    GSpan span = static_cast<const GVArrayImpl *>(this)->get_internal_span();
    return GMutableSpan(span.type(), const_cast<void *>(span.data()), span.size());
  }

  template<typename T> VMutableArrayImpl<T> *try_get_internal_mutable_varray()
  {
    BLI_assert(type_->is<T>());
    return (VMutableArrayImpl<T> *)this->try_get_internal_mutable_varray_impl();
  }

  /* Create a typed virtual array for this generic virtual array. */
  template<typename T> GVMutableArray_Typed<T> typed()
  {
    return GVMutableArray_Typed<T>(*this);
  }

  void fill(const void *value);

  /* Copy the values from the source buffer to all elements in the virtual array. */
  void set_all(const void *src)
  {
    this->set_all_impl(src);
  }

 protected:
  virtual void set_by_copy_impl(const int64_t index, const void *value);
  virtual void set_by_relocate_impl(const int64_t index, void *value);
  virtual void set_by_move_impl(const int64_t index, void *value) = 0;

  virtual void set_all_impl(const void *src);

  virtual void *try_get_internal_mutable_varray_impl();
};

class GVArray_For_GSpan : public GVArrayImpl {
 protected:
  const void *data_ = nullptr;
  const int64_t element_size_;

 public:
  GVArray_For_GSpan(const GSpan span)
      : GVArrayImpl(span.type(), span.size()),
        data_(span.data()),
        element_size_(span.type().size())
  {
  }

 protected:
  GVArray_For_GSpan(const CPPType &type, const int64_t size)
      : GVArrayImpl(type, size), element_size_(type.size())
  {
  }

  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  bool is_span_impl() const override;
  GSpan get_internal_span_impl() const override;
};

class GVArray_For_Empty : public GVArrayImpl {
 public:
  GVArray_For_Empty(const CPPType &type) : GVArrayImpl(type, 0)
  {
  }

 protected:
  void get_to_uninitialized_impl(const int64_t UNUSED(index), void *UNUSED(r_value)) const override
  {
    BLI_assert(false);
  }
};

class GVMutableArray_For_GMutableSpan : public GVMutableArrayImpl {
 protected:
  void *data_ = nullptr;
  const int64_t element_size_;

 public:
  GVMutableArray_For_GMutableSpan(const GMutableSpan span)
      : GVMutableArrayImpl(span.type(), span.size()),
        data_(span.data()),
        element_size_(span.type().size())
  {
  }

 protected:
  GVMutableArray_For_GMutableSpan(const CPPType &type, const int64_t size)
      : GVMutableArrayImpl(type, size), element_size_(type.size())
  {
  }

  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  void set_by_copy_impl(const int64_t index, const void *value) override;
  void set_by_move_impl(const int64_t index, void *value) override;
  void set_by_relocate_impl(const int64_t index, void *value) override;

  bool is_span_impl() const override;
  GSpan get_internal_span_impl() const override;
};

/* Generic virtual array where each element has the same value. The value is not owned. */
class GVArray_For_SingleValueRef : public GVArrayImpl {
 protected:
  const void *value_ = nullptr;

 public:
  GVArray_For_SingleValueRef(const CPPType &type, const int64_t size, const void *value)
      : GVArrayImpl(type, size), value_(value)
  {
  }

 protected:
  GVArray_For_SingleValueRef(const CPPType &type, const int64_t size) : GVArrayImpl(type, size)
  {
  }

  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  bool is_span_impl() const override;
  GSpan get_internal_span_impl() const override;

  bool is_single_impl() const override;
  void get_internal_single_impl(void *r_value) const override;
};

/* Same as GVArray_For_SingleValueRef, but the value is owned. */
class GVArray_For_SingleValue : public GVArray_For_SingleValueRef {
 public:
  GVArray_For_SingleValue(const CPPType &type, const int64_t size, const void *value);
  ~GVArray_For_SingleValue();
};

/* Used to convert a typed virtual array into a generic one. */
template<typename T> class GVArray_For_VArray : public GVArrayImpl {
 protected:
  const VArrayImpl<T> *varray_ = nullptr;
  VArray<T> local_varray_;

 public:
  GVArray_For_VArray(const VArrayImpl<T> &varray)
      : GVArrayImpl(CPPType::get<T>(), varray.size()), varray_(&varray)
  {
  }

  GVArray_For_VArray(VArray<T> varray) : local_varray_(std::move(varray))
  {
    BLI_assert(local_varray_);
    varray_ = &*local_varray_;
  }

 protected:
  GVArray_For_VArray(const int64_t size) : GVArrayImpl(CPPType::get<T>(), size)
  {
  }

  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = varray_->get(index);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(varray_->get(index));
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  GSpan get_internal_span_impl() const override
  {
    return GSpan(varray_->get_internal_span());
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  void get_internal_single_impl(void *r_value) const override
  {
    *(T *)r_value = varray_->get_internal_single();
  }

  void materialize_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize_to_uninitialized(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  const void *try_get_internal_varray_impl() const override
  {
    return varray_;
  }
};

class GVArray_For_GArray : public GVArray_For_GSpan {
 protected:
  GArray<> array_;

 public:
  GVArray_For_GArray(GArray<> array) : GVArray_For_GSpan(array.as_span()), array_(std::move(array))
  {
  }
};

class GVArray;

/* Used to convert any generic virtual array into a typed one. */
template<typename T> class VArray_For_GVArray : public VArrayImpl<T> {
 protected:
  const GVArrayImpl *varray_ = nullptr;
  /* TODO: Don't use #shared_ptr. */
  std::shared_ptr<GVArray> local_varray_;

 public:
  VArray_For_GVArray(const GVArrayImpl &varray) : VArrayImpl<T>(varray.size()), varray_(&varray)
  {
    BLI_assert(varray_->type().template is<T>());
  }

  VArray_For_GVArray(const GVArray &varray);

 protected:
  VArray_For_GVArray(const int64_t size) : VArrayImpl<T>(size)
  {
  }

  T get_impl(const int64_t index) const override
  {
    T value;
    varray_->get(index, &value);
    return value;
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  Span<T> get_internal_span_impl() const override
  {
    return varray_->get_internal_span().template typed<T>();
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  T get_internal_single_impl() const override
  {
    T value;
    varray_->get_internal_single(&value);
    return value;
  }
};

class GVMutableArray;

/* Used to convert an generic mutable virtual array into a typed one. */
template<typename T> class VMutableArray_For_GVMutableArray : public VMutableArrayImpl<T> {
 protected:
  GVMutableArrayImpl *varray_ = nullptr;
  std::shared_ptr<GVMutableArray> local_varray_;

 public:
  VMutableArray_For_GVMutableArray(GVMutableArrayImpl &varray)
      : VMutableArrayImpl<T>(varray.size()), varray_(&varray)
  {
    BLI_assert(varray.type().template is<T>());
  }

  VMutableArray_For_GVMutableArray(GVMutableArray varray);

  VMutableArray_For_GVMutableArray(const int64_t size) : VMutableArrayImpl<T>(size)
  {
  }

 private:
  T get_impl(const int64_t index) const override
  {
    T value;
    varray_->get(index, &value);
    return value;
  }

  void set_impl(const int64_t index, T value) override
  {
    varray_->set_by_relocate(index, &value);
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  Span<T> get_internal_span_impl() const override
  {
    return varray_->get_internal_span().template typed<T>();
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  T get_internal_single_impl() const override
  {
    T value;
    varray_->get_internal_single(&value);
    return value;
  }
};

/* Used to convert any typed virtual mutable array into a generic one. */
template<typename T> class GVMutableArray_For_VMutableArray : public GVMutableArrayImpl {
 protected:
  VMutableArrayImpl<T> *varray_ = nullptr;
  VMutableArray<T> local_varray_;

 public:
  GVMutableArray_For_VMutableArray(VMutableArrayImpl<T> &varray)
      : GVMutableArrayImpl(CPPType::get<T>(), varray.size()), varray_(&varray)
  {
  }

  GVMutableArray_For_VMutableArray(VMutableArray<T> varray) : local_varray_(std::move(varray))
  {
    BLI_assert(local_varray_);
    varray_ = &*local_varray_;
  }

 protected:
  GVMutableArray_For_VMutableArray(const int64_t size)
      : GVMutableArrayImpl(CPPType::get<T>(), size)
  {
  }

  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = varray_->get(index);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(varray_->get(index));
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  GSpan get_internal_span_impl() const override
  {
    Span<T> span = varray_->get_internal_span();
    return span;
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  void get_internal_single_impl(void *r_value) const override
  {
    *(T *)r_value = varray_->get_internal_single();
  }

  void set_by_copy_impl(const int64_t index, const void *value) override
  {
    const T &value_ = *(const T *)value;
    varray_->set(index, value_);
  }

  void set_by_relocate_impl(const int64_t index, void *value) override
  {
    T &value_ = *(T *)value;
    varray_->set(index, std::move(value_));
    value_.~T();
  }

  void set_by_move_impl(const int64_t index, void *value) override
  {
    T &value_ = *(T *)value;
    varray_->set(index, std::move(value_));
  }

  void set_all_impl(const void *src) override
  {
    varray_->set_all(Span((T *)src, size_));
  }

  void materialize_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize_to_uninitialized(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  const void *try_get_internal_varray_impl() const override
  {
    return (const VArrayImpl<T> *)varray_;
  }

  void *try_get_internal_mutable_varray_impl() override
  {
    return varray_;
  }
};

/* A generic version of VArray_Span. */
class GVArray_GSpan : public GSpan {
 private:
  const GVArrayImpl &varray_;
  void *owned_data_ = nullptr;

 public:
  GVArray_GSpan(const GVArrayImpl &varray);
  ~GVArray_GSpan();
};

/* A generic version of VMutableArray_Span. */
class GVMutableArray_GSpan : public GMutableSpan {
 private:
  GVMutableArrayImpl &varray_;
  void *owned_data_ = nullptr;
  bool save_has_been_called_ = false;
  bool show_not_saved_warning_ = true;

 public:
  GVMutableArray_GSpan(GVMutableArrayImpl &varray, bool copy_values_to_span = true);
  ~GVMutableArray_GSpan();

  void save();
  void disable_not_applied_warning();
};

/* Similar to GVArray_GSpan, but the resulting span is typed. */
template<typename T> class GVArray_Span : public Span<T> {
 private:
  GVArray_GSpan varray_gspan_;

 public:
  GVArray_Span(const GVArrayImpl &varray) : varray_gspan_(varray)
  {
    BLI_assert(varray.type().is<T>());
    this->data_ = (const T *)varray_gspan_.data();
    this->size_ = varray_gspan_.size();
  }
};

template<typename T> class GVArray_For_OwnedVArray : public GVArray_For_VArray<T> {
 private:
  VArrayPtr<T> owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  GVArray_For_OwnedVArray(VArrayPtr<T> varray)
      : GVArray_For_VArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

template<typename T> class VArray_For_OwnedGVArray : public VArray_For_GVArray<T> {
 private:
  GVArrayPtr owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  VArray_For_OwnedGVArray(GVArrayPtr varray)
      : VArray_For_GVArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

template<typename T>
class GVMutableArray_For_OwnedVMutableArray : public GVMutableArray_For_VMutableArray<T> {
 private:
  VMutableArrayPtr<T> owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  GVMutableArray_For_OwnedVMutableArray(VMutableArrayPtr<T> varray)
      : GVMutableArray_For_VMutableArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

template<typename T>
class VMutableArray_For_OwnedGVMutableArray : public VMutableArray_For_GVMutableArray<T> {
 private:
  GVMutableArrayPtr owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  VMutableArray_For_OwnedGVMutableArray(GVMutableArrayPtr varray)
      : VMutableArray_For_GVMutableArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

/* Utility to embed a typed virtual array into a generic one. This avoids one allocation and give
 * the compiler more opportunity to optimize the generic virtual array. */
template<typename T, typename VArrayT>
class GVArray_For_EmbeddedVArray : public GVArray_For_VArray<T> {
 private:
  VArrayT embedded_varray_;

 public:
  template<typename... Args>
  GVArray_For_EmbeddedVArray(const int64_t size, Args &&...args)
      : GVArray_For_VArray<T>(size), embedded_varray_(std::forward<Args>(args)...)
  {
    this->varray_ = &embedded_varray_;
  }
};

/* Same as GVArray_For_EmbeddedVArray, but for mutable virtual arrays. */
template<typename T, typename VMutableArrayT>
class GVMutableArray_For_EmbeddedVMutableArray : public GVMutableArray_For_VMutableArray<T> {
 private:
  VMutableArrayT embedded_varray_;

 public:
  template<typename... Args>
  GVMutableArray_For_EmbeddedVMutableArray(const int64_t size, Args &&...args)
      : GVMutableArray_For_VMutableArray<T>(size), embedded_varray_(std::forward<Args>(args)...)
  {
    this->varray_ = &embedded_varray_;
  }
};

/* Same as VArray_For_ArrayContainer, but for a generic virtual array. */
template<typename Container, typename T = typename Container::value_type>
class GVArray_For_ArrayContainer
    : public GVArray_For_EmbeddedVArray<T, VArray_For_ArrayContainer<Container, T>> {
 public:
  GVArray_For_ArrayContainer(Container container)
      : GVArray_For_EmbeddedVArray<T, VArray_For_ArrayContainer<Container, T>>(
            container.size(), std::move(container))
  {
  }
};

/* Same as VArray_For_DerivedSpan, but for a generic virtual array. */
template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class GVArray_For_DerivedSpan
    : public GVArray_For_EmbeddedVArray<ElemT, VArray_For_DerivedSpan<StructT, ElemT, GetFunc>> {
 public:
  GVArray_For_DerivedSpan(const Span<StructT> data)
      : GVArray_For_EmbeddedVArray<ElemT, VArray_For_DerivedSpan<StructT, ElemT, GetFunc>>(
            data.size(), data)
  {
  }
};

/* Same as VMutableArray_For_DerivedSpan, but for a generic virtual array. */
template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, ElemT)>
class GVMutableArray_For_DerivedSpan
    : public GVMutableArray_For_EmbeddedVMutableArray<
          ElemT,
          VMutableArray_For_DerivedSpan<StructT, ElemT, GetFunc, SetFunc>> {
 public:
  GVMutableArray_For_DerivedSpan(const MutableSpan<StructT> data)
      : GVMutableArray_For_EmbeddedVMutableArray<
            ElemT,
            VMutableArray_For_DerivedSpan<StructT, ElemT, GetFunc, SetFunc>>(data.size(), data)
  {
  }
};

/* Same as VArray_For_Span, but for a generic virtual array. */
template<typename T>
class GVArray_For_Span : public GVArray_For_EmbeddedVArray<T, VArray_For_Span<T>> {
 public:
  GVArray_For_Span(const Span<T> data)
      : GVArray_For_EmbeddedVArray<T, VArray_For_Span<T>>(data.size(), data)
  {
  }
};

/* Same as VMutableArray_For_MutableSpan, but for a generic virtual array. */
template<typename T>
class GVMutableArray_For_MutableSpan
    : public GVMutableArray_For_EmbeddedVMutableArray<T, VMutableArray_For_MutableSpan<T>> {
 public:
  GVMutableArray_For_MutableSpan(const MutableSpan<T> data)
      : GVMutableArray_For_EmbeddedVMutableArray<T, VMutableArray_For_MutableSpan<T>>(data.size(),
                                                                                      data)
  {
  }
};

/**
 * Utility class to create the "best" typed virtual array for a given generic virtual array.
 * In most cases we don't just want to use VArray_For_GVArray, because it adds an additional
 * indirection on element-access that can be avoided in many cases (e.g. when the virtual array is
 * just a span or single value).
 *
 * This is not a virtual array itself, but is used to get a virtual array.
 */
template<typename T> class GVArray_Typed {
 private:
  const VArrayImpl<T> *varray_;
  /* Of these optional virtual arrays, at most one is constructed at any time. */
  std::optional<VArray_For_Span<T>> varray_span_;
  std::optional<VArray_For_Single<T>> varray_single_;
  std::optional<VArray_For_GVArray<T>> varray_any_;
  GVArrayPtr owned_gvarray_;

 public:
  explicit GVArray_Typed(const GVArrayImpl &gvarray)
  {
    BLI_assert(gvarray.type().is<T>());
    if (gvarray.is_span()) {
      const GSpan span = gvarray.get_internal_span();
      varray_span_.emplace(span.typed<T>());
      varray_ = &*varray_span_;
    }
    else if (gvarray.is_single()) {
      T single_value;
      gvarray.get_internal_single(&single_value);
      varray_single_.emplace(single_value, gvarray.size());
      varray_ = &*varray_single_;
    }
    else if (const VArrayImpl<T> *internal_varray = gvarray.try_get_internal_varray<T>()) {
      varray_ = internal_varray;
    }
    else {
      varray_any_.emplace(gvarray);
      varray_ = &*varray_any_;
    }
  }

  /* Same as the constructor above, but also takes ownership of the passed in virtual array. */
  explicit GVArray_Typed(GVArrayPtr gvarray) : GVArray_Typed(*gvarray)
  {
    owned_gvarray_ = std::move(gvarray);
  }

  const VArrayImpl<T> &operator*() const
  {
    return *varray_;
  }

  const VArrayImpl<T> *operator->() const
  {
    return varray_;
  }

  /* Support implicit cast to the typed virtual array for convenience when `varray->typed<T>()` is
   * used within an expression. */
  operator const VArrayImpl<T> &() const
  {
    return *varray_;
  }

  T operator[](const int64_t index) const
  {
    return varray_->get(index);
  }

  int64_t size() const
  {
    return varray_->size();
  }

  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }
};

/* Same as GVArray_Typed, but for mutable virtual arrays. */
template<typename T> class GVMutableArray_Typed {
 private:
  VMutableArrayImpl<T> *varray_;
  std::optional<VMutableArray_For_MutableSpan<T>> varray_span_;
  std::optional<VMutableArray_For_GVMutableArray<T>> varray_any_;
  GVMutableArrayPtr owned_gvarray_;

 public:
  explicit GVMutableArray_Typed(GVMutableArrayImpl &gvarray)
  {
    BLI_assert(gvarray.type().is<T>());
    if (gvarray.is_span()) {
      const GMutableSpan span = gvarray.get_internal_span();
      varray_span_.emplace(span.typed<T>());
      varray_ = &*varray_span_;
    }
    else if (VMutableArrayImpl<T> *internal_varray =
                 gvarray.try_get_internal_mutable_varray<T>()) {
      varray_ = internal_varray;
    }
    else {
      varray_any_.emplace(gvarray);
      varray_ = &*varray_any_;
    }
  }

  explicit GVMutableArray_Typed(GVMutableArrayPtr gvarray) : GVMutableArray_Typed(*gvarray)
  {
    owned_gvarray_ = std::move(gvarray);
  }

  VMutableArrayImpl<T> &operator*()
  {
    return *varray_;
  }

  VMutableArrayImpl<T> *operator->()
  {
    return varray_;
  }

  operator VMutableArrayImpl<T> &()
  {
    return *varray_;
  }

  T operator[](const int64_t index) const
  {
    return varray_->get(index);
  }

  int64_t size() const
  {
    return varray_->size();
  }
};

class GVArray_For_SlicedGVArray : public GVArrayImpl {
 protected:
  const GVArrayImpl &varray_;
  int64_t offset_;

 public:
  GVArray_For_SlicedGVArray(const GVArrayImpl &varray, const IndexRange slice)
      : GVArrayImpl(varray.type(), slice.size()), varray_(varray), offset_(slice.start())
  {
    BLI_assert(slice.one_after_last() <= varray.size());
  }

  /* TODO: Add #materialize method. */
  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;
};

/**
 * Utility class to create the "best" sliced virtual array.
 */
class GVArray_Slice {
 private:
  const GVArrayImpl *varray_;
  /* Of these optional virtual arrays, at most one is constructed at any time. */
  std::optional<GVArray_For_GSpan> varray_span_;
  std::optional<GVArray_For_SlicedGVArray> varray_any_;

 public:
  GVArray_Slice(const GVArrayImpl &varray, const IndexRange slice);

  const GVArrayImpl &operator*()
  {
    return *varray_;
  }

  const GVArrayImpl *operator->()
  {
    return varray_;
  }

  operator const GVArrayImpl &()
  {
    return *varray_;
  }
};

namespace detail {

struct GVArrayAnyExtraInfo {
  const GVArrayImpl *(*get_varray)(const void *buffer) =
      [](const void *UNUSED(buffer)) -> const GVArrayImpl * { return nullptr; };

  template<typename StorageT> static GVArrayAnyExtraInfo get()
  {
    static_assert(std::is_base_of_v<GVArrayImpl, StorageT> ||
                  std::is_same_v<StorageT, std::shared_ptr<const GVArrayImpl>>);

    if constexpr (std::is_base_of_v<GVArrayImpl, StorageT>) {
      return {[](const void *buffer) {
        return static_cast<const GVArrayImpl *>((const StorageT *)buffer);
      }};
    }
    else if constexpr (std::is_same_v<StorageT, std::shared_ptr<const GVArrayImpl>>) {
      return {[](const void *buffer) { return ((const StorageT *)buffer)->get(); }};
    }
    else {
      BLI_assert_unreachable();
      return {};
    }
  }
};

}  // namespace detail

class GVArray {
 private:
  using ExtraInfo = detail::GVArrayAnyExtraInfo;
  using Storage = Any<ExtraInfo, 32, 8>;
  using Impl = GVArrayImpl;

  const Impl *impl_ = nullptr;
  Storage storage_;

 public:
  GVArray() = default;

  GVArray(const GVArray &other) : storage_(other.storage_)
  {
    impl_ = storage_.extra_info().get_varray(storage_.get());
  }

  GVArray(const Impl *impl) : impl_(impl)
  {
  }

  GVArray(std::shared_ptr<const Impl> impl) : impl_(impl.get())
  {
    if (impl) {
      storage_ = std::move(impl);
    }
  }

  template<typename T> GVArray(const VArray<T> &varray)
  {
    if (!varray) {
      return;
    }
    if (varray->is_span()) {
      Span<T> data = varray->get_internal_span();
      *this = GVArray::ForSpan(data);
    }
    else if (varray->is_single()) {
      T value = varray->get_internal_single();
      *this = GVArray::ForSingle(CPPType::get<T>(), varray->size(), &value);
    }
    else {
      *this = GVArray::For<GVArray_For_VArray<T>>(varray);
    }
  }

  template<typename T> VArray<T> typed() const
  {
    if (*this) {
      return {};
    }
    BLI_assert(impl_->type().is<T>());
    if (impl_->is_span()) {
      const GSpan span = impl_->get_internal_span();
      return VArray<T>::ForSpan(span.typed<T>());
    }
    if (impl_->is_single()) {
      T value;
      impl_->get_internal_single(&value);
      return VArray<T>::ForSingle(value, impl_->size());
    }
    return VArray<T>::template For<VArray_For_GVArray<T>>(*this);
  }

  template<typename ImplT, typename... Args> static GVArray For(Args &&...args)
  {
    static_assert(std::is_base_of_v<Impl, ImplT>);
    if constexpr (std::is_copy_constructible_v<ImplT> && Storage::template is_inline_v<ImplT>) {
      GVArray varray;
      varray.impl_ = &varray.storage_.template emplace<ImplT>(std::forward<Args>(args)...);
      return varray;
    }
    else {
      return GVArray(std::make_shared<ImplT>(std::forward<Args>(args)...));
    }
  }

  static GVArray ForSingleRef(const CPPType &type, const int64_t size, const void *value);
  static GVArray ForSingle(const CPPType &type, const int64_t size, const void *value);
  static GVArray ForSpan(GSpan span);

  operator bool() const
  {
    return impl_ != nullptr;
  }

  const Impl *operator->() const
  {
    BLI_assert(*this);
    return impl_;
  }

  const Impl &operator*() const
  {
    BLI_assert(*this);
    return *impl_;
  }
};

class GVMutableArray {
 private:
  using ExtraInfo = detail::GVArrayAnyExtraInfo;
  using Storage = Any<ExtraInfo, 32, 8>;
  using Impl = GVMutableArrayImpl;

  Impl *impl_ = nullptr;
  Storage storage_;

 public:
  GVMutableArray() = default;

  GVMutableArray(const GVMutableArray &other) : storage_(other.storage_)
  {
    impl_ = const_cast<Impl *>(
        static_cast<const Impl *>(storage_.extra_info().get_varray(storage_.get())));
  }

  GVMutableArray(Impl *impl) : impl_(impl)
  {
  }

  GVMutableArray(std::shared_ptr<Impl> impl) : impl_(impl.get())
  {
    if (impl) {
      storage_ = std::shared_ptr<const GVArrayImpl>(std::move(impl));
    }
  }

  template<typename T> GVMutableArray(VMutableArray<T> &varray)
  {
    if (!varray) {
      return;
    }
    if (varray->is_span()) {
      Span<T> data = varray->get_internal_span();
      *this = GVMutableArray::ForSpan(data);
    }
    else {
      *this = GVMutableArray::For<GVMutableArray_For_VMutableArray<T>>(varray);
    }
  }

  template<typename T> VMutableArray<T> typed() const
  {
    if (*this) {
      return {};
    }
    BLI_assert(impl_->type().is<T>());
    if (impl_->is_span()) {
      const GSpan span = impl_->get_internal_span();
      return VMutableArray<T>::ForSpan(span.typed<T>());
    }
    return VArray<T>::template For<VMutableArray_For_GVMutableArray<T>>(*this);
  }

  template<typename ImplT, typename... Args> static GVMutableArray For(Args &&...args)
  {
    static_assert(std::is_base_of_v<Impl, ImplT>);
    if constexpr (std::is_copy_constructible_v<ImplT> && Storage::template is_inline_v<ImplT>) {
      GVMutableArray varray;
      varray.impl_ = &varray.storage_.template emplace<ImplT>(std::forward<Args>(args)...);
      return varray;
    }
    else {
      return GVMutableArray(std::make_shared<ImplT>(std::forward<Args>(args)...));
    }
  }

  static GVMutableArray ForSpan(GMutableSpan span);

  operator bool() const
  {
    return impl_ != nullptr;
  }

  Impl *operator->()
  {
    BLI_assert(*this);
    return impl_;
  }

  const Impl *operator->() const
  {
    BLI_assert(*this);
    return impl_;
  }

  Impl &operator*()
  {
    BLI_assert(*this);
    return *impl_;
  }

  const Impl &operator*() const
  {
    BLI_assert(*this);
    return *impl_;
  }
};

template<typename T>
inline VArray_For_GVArray<T>::VArray_For_GVArray(const GVArray &varray)
    : local_varray_(std::make_shared<GVArray>(std::move(varray)))
{
  BLI_assert(*local_varray_);
  varray_ = &**local_varray_;
}

template<typename T>
inline VMutableArray_For_GVMutableArray<T>::VMutableArray_For_GVMutableArray(GVMutableArray varray)
    : local_varray_(std::make_shared<GVMutableArray>(std::move(varray)))
{
  BLI_assert(*local_varray_);
  varray_ = &**local_varray_;
}

}  // namespace blender::fn
