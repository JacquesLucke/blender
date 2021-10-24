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

#include "FN_generic_virtual_array.hh"

namespace blender::fn {

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl
 * \{ */

void GVArrayImpl::materialize(void *dst) const
{
  this->materialize(IndexMask(size_), dst);
}

void GVArrayImpl::materialize(const IndexMask mask, void *dst) const
{
  this->materialize_impl(mask, dst);
}

void GVArrayImpl::materialize_impl(const IndexMask mask, void *dst) const
{
  for (const int64_t i : mask) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
    this->get(i, elem_dst);
  }
}

void GVArrayImpl::materialize_to_uninitialized(void *dst) const
{
  this->materialize_to_uninitialized(IndexMask(size_), dst);
}

void GVArrayImpl::materialize_to_uninitialized(const IndexMask mask, void *dst) const
{
  BLI_assert(mask.min_array_size() <= size_);
  this->materialize_to_uninitialized_impl(mask, dst);
}

void GVArrayImpl::materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const
{
  for (const int64_t i : mask) {
    void *elem_dst = POINTER_OFFSET(dst, type_->size() * i);
    this->get_to_uninitialized(i, elem_dst);
  }
}

void GVArrayImpl::get_impl(const int64_t index, void *r_value) const
{
  type_->destruct(r_value);
  this->get_to_uninitialized_impl(index, r_value);
}

bool GVArrayImpl::is_span_impl() const
{
  return false;
}

GSpan GVArrayImpl::get_internal_span_impl() const
{
  BLI_assert(false);
  return GSpan(*type_);
}

bool GVArrayImpl::is_single_impl() const
{
  return false;
}

void GVArrayImpl::get_internal_single_impl(void *UNUSED(r_value)) const
{
  BLI_assert(false);
}

const void *GVArrayImpl::try_get_internal_varray_impl() const
{
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVMutableArrayImpl
 * \{ */

void GVMutableArrayImpl::set_by_copy_impl(const int64_t index, const void *value)
{
  BUFFER_FOR_CPP_TYPE_VALUE(*type_, buffer);
  type_->copy_construct(value, buffer);
  this->set_by_move_impl(index, buffer);
  type_->destruct(buffer);
}

void GVMutableArrayImpl::set_by_relocate_impl(const int64_t index, void *value)
{
  this->set_by_move_impl(index, value);
  type_->destruct(value);
}

void GVMutableArrayImpl::set_all_impl(const void *src)
{
  if (this->is_span()) {
    const GMutableSpan span = this->get_internal_span();
    type_->copy_assign_n(src, span.data(), size_);
  }
  else {
    for (int64_t i : IndexRange(size_)) {
      this->set_by_copy(i, POINTER_OFFSET(src, type_->size() * i));
    }
  }
}

void *GVMutableArrayImpl::try_get_internal_mutable_varray_impl()
{
  return nullptr;
}

void GVMutableArrayImpl::fill(const void *value)
{
  if (this->is_span()) {
    const GMutableSpan span = this->get_internal_span();
    type_->fill_assign_n(value, span.data(), size_);
  }
  else {
    for (int64_t i : IndexRange(size_)) {
      this->set_by_copy(i, value);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_GSpan
 * \{ */

void GVArrayImpl_For_GSpan::get_impl(const int64_t index, void *r_value) const
{
  type_->copy_assign(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVArrayImpl_For_GSpan::get_to_uninitialized_impl(const int64_t index, void *r_value) const
{
  type_->copy_construct(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

bool GVArrayImpl_For_GSpan::is_span_impl() const
{
  return true;
}

GSpan GVArrayImpl_For_GSpan::get_internal_span_impl() const
{
  return GSpan(*type_, data_, size_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVMutableArrayImpl_For_GMutableSpan
 * \{ */

void GVMutableArrayImpl_For_GMutableSpan::get_impl(const int64_t index, void *r_value) const
{
  type_->copy_assign(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVMutableArrayImpl_For_GMutableSpan::get_to_uninitialized_impl(const int64_t index,
                                                                    void *r_value) const
{
  type_->copy_construct(POINTER_OFFSET(data_, element_size_ * index), r_value);
}

void GVMutableArrayImpl_For_GMutableSpan::set_by_copy_impl(const int64_t index, const void *value)
{
  type_->copy_assign(value, POINTER_OFFSET(data_, element_size_ * index));
}

void GVMutableArrayImpl_For_GMutableSpan::set_by_move_impl(const int64_t index, void *value)
{
  type_->move_construct(value, POINTER_OFFSET(data_, element_size_ * index));
}

void GVMutableArrayImpl_For_GMutableSpan::set_by_relocate_impl(const int64_t index, void *value)
{
  type_->relocate_assign(value, POINTER_OFFSET(data_, element_size_ * index));
}

bool GVMutableArrayImpl_For_GMutableSpan::is_span_impl() const
{
  return true;
}

GSpan GVMutableArrayImpl_For_GMutableSpan::get_internal_span_impl() const
{
  return GSpan(*type_, data_, size_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SingleValueRef
 * \{ */

/* Generic virtual array where each element has the same value. The value is not owned. */
class GVArrayImpl_For_SingleValueRef : public GVArrayImpl {
 protected:
  const void *value_ = nullptr;

 public:
  GVArrayImpl_For_SingleValueRef(const CPPType &type, const int64_t size, const void *value)
      : GVArrayImpl(type, size), value_(value)
  {
  }

 protected:
  GVArrayImpl_For_SingleValueRef(const CPPType &type, const int64_t size) : GVArrayImpl(type, size)
  {
  }

  void get_impl(const int64_t UNUSED(index), void *r_value) const override
  {
    type_->copy_assign(value_, r_value);
  }
  void get_to_uninitialized_impl(const int64_t UNUSED(index), void *r_value) const override
  {
    type_->copy_construct(value_, r_value);
  }

  bool is_span_impl() const override
  {
    return size_ == 1;
  }
  GSpan get_internal_span_impl() const override
  {
    return GSpan{*type_, value_, 1};
  }

  bool is_single_impl() const override
  {
    return true;
  }
  void get_internal_single_impl(void *r_value) const override
  {
    type_->copy_assign(value_, r_value);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SingleValue
 * \{ */

/* Same as GVArrayImpl_For_SingleValueRef, but the value is owned. */
class GVArrayImpl_For_SingleValue : public GVArrayImpl_For_SingleValueRef {
 public:
  GVArrayImpl_For_SingleValue(const CPPType &type, const int64_t size, const void *value)
      : GVArrayImpl_For_SingleValueRef(type, size)
  {
    value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_construct(value, (void *)value_);
  }

  ~GVArrayImpl_For_SingleValue()
  {
    type_->destruct((void *)value_);
    MEM_freeN((void *)value_);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArray_GSpan
 * \{ */

GVArray_GSpan::GVArray_GSpan(const GVArrayImpl &varray) : GSpan(varray.type()), varray_(varray)
{
  size_ = varray_.size();
  if (varray_.is_span()) {
    data_ = varray_.get_internal_span().data();
  }
  else {
    owned_data_ = MEM_mallocN_aligned(type_->size() * size_, type_->alignment(), __func__);
    varray_.materialize_to_uninitialized(IndexRange(size_), owned_data_);
    data_ = owned_data_;
  }
}

GVArray_GSpan::~GVArray_GSpan()
{
  if (owned_data_ != nullptr) {
    type_->destruct_n(owned_data_, size_);
    MEM_freeN(owned_data_);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVMutableArray_GSpan
 * \{ */

GVMutableArray_GSpan::GVMutableArray_GSpan(GVMutableArrayImpl &varray,
                                           const bool copy_values_to_span)
    : GMutableSpan(varray.type()), varray_(varray)
{
  size_ = varray_.size();
  if (varray_.is_span()) {
    data_ = varray_.get_internal_span().data();
  }
  else {
    owned_data_ = MEM_mallocN_aligned(type_->size() * size_, type_->alignment(), __func__);
    if (copy_values_to_span) {
      varray_.materialize_to_uninitialized(IndexRange(size_), owned_data_);
    }
    else {
      type_->default_construct_n(owned_data_, size_);
    }
    data_ = owned_data_;
  }
}

GVMutableArray_GSpan::~GVMutableArray_GSpan()
{
  if (show_not_saved_warning_) {
    if (!save_has_been_called_) {
      std::cout << "Warning: Call `apply()` to make sure that changes persist in all cases.\n";
    }
  }
  if (owned_data_ != nullptr) {
    type_->destruct_n(owned_data_, size_);
    MEM_freeN(owned_data_);
  }
}

void GVMutableArray_GSpan::save()
{
  save_has_been_called_ = true;
  if (data_ != owned_data_) {
    return;
  }
  const int64_t element_size = type_->size();
  for (int64_t i : IndexRange(size_)) {
    varray_.set_by_copy(i, POINTER_OFFSET(owned_data_, element_size * i));
  }
}

void GVMutableArray_GSpan::disable_not_applied_warning()
{
  show_not_saved_warning_ = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_SlicedGVArray
 * \{ */

class GVArrayImpl_For_SlicedGVArray : public GVArrayImpl {
 protected:
  GVArray varray_;
  int64_t offset_;

 public:
  GVArrayImpl_For_SlicedGVArray(GVArray varray, const IndexRange slice)
      : GVArrayImpl(varray->type(), slice.size()),
        varray_(std::move(varray)),
        offset_(slice.start())
  {
    BLI_assert(slice.one_after_last() <= varray->size());
  }

  void get_impl(const int64_t index, void *r_value) const override
  {
    varray_->get(index + offset_, r_value);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    varray_->get_to_uninitialized(index + offset_, r_value);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArray
 * \{ */

GVArray GVArray::ForSingle(const CPPType &type, const int64_t size, const void *value)
{
  return GVArray::For<GVArrayImpl_For_SingleValue>(type, size, value);
}

GVArray GVArray::ForSingleRef(const CPPType &type, const int64_t size, const void *value)
{
  return GVArray::For<GVArrayImpl_For_SingleValueRef>(type, size, value);
}

GVArray GVArray::ForSingleDefault(const CPPType &type, const int64_t size)
{
  return GVArray::ForSingleRef(type, size, type.default_value());
}

GVArray GVArray::ForSpan(GSpan span)
{
  return GVArray::For<GVArrayImpl_For_GSpan>(span);
}

class GVArrayImpl_For_GArray : public GVArrayImpl_For_GSpan {
 protected:
  GArray<> array_;

 public:
  GVArrayImpl_For_GArray(GArray<> array)
      : GVArrayImpl_For_GSpan(array.as_span()), array_(std::move(array))
  {
  }
};

GVArray GVArray::ForGArray(GArray<> array)
{
  return GVArray::For<GVArrayImpl_For_GArray>(array);
}

GVArray GVArray::ForEmpty(const CPPType &type)
{
  return GVArray::ForSpan(GSpan(type));
}

GVArray GVArray::slice(IndexRange slice) const
{
  return GVArray::For<GVArrayImpl_For_SlicedGVArray>(*this, slice);
}

GVArray &GVArray::operator=(const GVArray &other)
{
  if (this == &other) {
    return *this;
  }
  this->~GVArray();
  new (this) GVArray(other);
  return *this;
}

GVArray &GVArray::operator=(GVArray &&other)
{
  if (this == &other) {
    return *this;
  }
  this->~GVArray();
  new (this) GVArray(std::move(other));
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVMutableArray
 * \{ */

GVMutableArray GVMutableArray::ForSpan(GMutableSpan span)
{
  return GVMutableArray::For<GVMutableArrayImpl_For_GMutableSpan>(span);
}

GVMutableArray::operator GVArray() const
{
  GVArray varray;
  varray.impl_ = impl_;
  varray.storage_ = storage_;
  return varray;
}

GVMutableArray &GVMutableArray::operator=(const GVMutableArray &other)
{
  if (this == &other) {
    return *this;
  }
  this->~GVMutableArray();
  new (this) GVMutableArray(other);
  return *this;
}

GVMutableArray &GVMutableArray::operator=(GVMutableArray &&other)
{
  if (this == &other) {
    return *this;
  }
  this->~GVMutableArray();
  new (this) GVMutableArray(std::move(other));
  return *this;
}

/** \} */

}  // namespace blender::fn
