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
 *
 * A virtual array is a data structure that behaves similar to an array, but its elements are
 * accessed through virtual methods. This improves the decoupling of a function from its callers,
 * because it does not have to know exactly how the data is laid out in memory, or if it is stored
 * in memory at all. It could just as well be computed on the fly.
 *
 * Taking a virtual array as parameter instead of a more specific non-virtual type has some
 * tradeoffs. Access to individual elements of the individual elements is higher due to function
 * call overhead. On the other hand, potential callers don't have to convert the data into the
 * specific format required for the function. This can be a costly conversion if only few of the
 * elements are accessed in the end.
 *
 * Functions taking a virtual array as input can still optimize for different data layouts. For
 * example, they can check if the array is stored as an array internally or if it is the same
 * element for all indices. Whether it is worth to optimize for different data layouts in a
 * function has to be decided on a case by case basis. One should always do some benchmarking to
 * see of the increased compile time and binary size is worth it.
 */

#include "BLI_any.hh"
#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_span.hh"

namespace blender {

/* An immutable virtual array. */
template<typename T> class VArrayImpl {
 protected:
  int64_t size_;

 public:
  VArrayImpl(const int64_t size) : size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~VArrayImpl() = default;

  T get(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_impl(index);
  }

  int64_t size() const
  {
    return size_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  IndexRange index_range() const
  {
    return IndexRange(size_);
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
  Span<T> get_internal_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return {};
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

  /* Returns the value that is returned for every index. This invokes undefined behavior if the
   * virtual array would not return the same value for every index. */
  T get_internal_single() const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      return this->get(0);
    }
    return this->get_internal_single_impl();
  }

  /* Get the element at a specific index. Note that this operator cannot be used to assign values
   * to an index, because the return value is not a reference. */
  T operator[](const int64_t index) const
  {
    return this->get(index);
  }

  /* Copy the entire virtual array into a span. */
  void materialize(MutableSpan<T> r_span) const
  {
    this->materialize(IndexMask(size_), r_span);
  }

  /* Copy some indices of the virtual array into a span. */
  void materialize(IndexMask mask, MutableSpan<T> r_span) const
  {
    BLI_assert(mask.min_array_size() <= size_);
    this->materialize_impl(mask, r_span);
  }

  void materialize_to_uninitialized(MutableSpan<T> r_span) const
  {
    this->materialize_to_uninitialized(IndexMask(size_), r_span);
  }

  void materialize_to_uninitialized(IndexMask mask, MutableSpan<T> r_span) const
  {
    BLI_assert(mask.min_array_size() <= size_);
    this->materialize_to_uninitialized_impl(mask, r_span);
  }

 protected:
  virtual T get_impl(const int64_t index) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual Span<T> get_internal_span_impl() const
  {
    BLI_assert_unreachable();
    return {};
  }

  virtual bool is_single_impl() const
  {
    return false;
  }

  virtual T get_internal_single_impl() const
  {
    /* Provide a default implementation, so that subclasses don't have to provide it. This method
     * should never be called because `is_single_impl` returns false by default. */
    BLI_assert_unreachable();
    return T();
  }

  virtual void materialize_impl(IndexMask mask, MutableSpan<T> r_span) const
  {
    T *dst = r_span.data();
    if (this->is_span()) {
      const T *src = this->get_internal_span().data();
      mask.foreach_index([&](const int64_t i) { dst[i] = src[i]; });
    }
    else if (this->is_single()) {
      const T single = this->get_internal_single();
      mask.foreach_index([&](const int64_t i) { dst[i] = single; });
    }
    else {
      mask.foreach_index([&](const int64_t i) { dst[i] = this->get(i); });
    }
  }

  virtual void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<T> r_span) const
  {
    T *dst = r_span.data();
    if (this->is_span()) {
      const T *src = this->get_internal_span().data();
      mask.foreach_index([&](const int64_t i) { new (dst + i) T(src[i]); });
    }
    else if (this->is_single()) {
      const T single = this->get_internal_single();
      mask.foreach_index([&](const int64_t i) { new (dst + i) T(single); });
    }
    else {
      mask.foreach_index([&](const int64_t i) { new (dst + i) T(this->get(i)); });
    }
  }
};

/* Similar to VArrayImpl, but the elements are mutable. */
template<typename T> class VMutableArrayImpl : public VArrayImpl<T> {
 public:
  VMutableArrayImpl(const int64_t size) : VArrayImpl<T>(size)
  {
  }

  void set(const int64_t index, T value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size_);
    this->set_impl(index, std::move(value));
  }

  /* Copy the values from the source span to all elements in the virtual array. */
  void set_all(Span<T> src)
  {
    BLI_assert(src.size() == this->size_);
    this->set_all_impl(src);
  }

  MutableSpan<T> get_internal_span()
  {
    BLI_assert(this->is_span());
    Span<T> span = static_cast<const VArrayImpl<T> *>(this)->get_internal_span();
    return MutableSpan<T>(const_cast<T *>(span.data()), span.size());
  }

 protected:
  virtual void set_impl(const int64_t index, T value) = 0;

  virtual void set_all_impl(Span<T> src)
  {
    if (this->is_span()) {
      const MutableSpan<T> span = this->get_internal_span();
      initialized_copy_n(src.data(), this->size_, span.data());
    }
    else {
      const int64_t size = this->size_;
      for (int64_t i = 0; i < size; i++) {
        this->set(i, src[i]);
      }
    }
  }
};

template<typename T> using VArrayPtr = std::unique_ptr<VArrayImpl<T>>;
template<typename T> using VMutableArrayPtr = std::unique_ptr<VMutableArrayImpl<T>>;

/**
 * A virtual array implementation for a span. Methods in this class are final so that it can be
 * devirtualized by the compiler in some cases (e.g. when #devirtualize_varray is used).
 */
template<typename T> class VArrayImpl_For_Span : public VArrayImpl<T> {
 protected:
  const T *data_ = nullptr;

 public:
  VArrayImpl_For_Span(const Span<T> data) : VArrayImpl<T>(data.size()), data_(data.data())
  {
  }

 protected:
  VArrayImpl_For_Span(const int64_t size) : VArrayImpl<T>(size)
  {
  }

  T get_impl(const int64_t index) const final
  {
    return data_[index];
  }

  bool is_span_impl() const final
  {
    return true;
  }

  Span<T> get_internal_span_impl() const final
  {
    return Span<T>(data_, this->size_);
  }
};

template<typename T> class VMutableArrayImpl_For_MutableSpan : public VMutableArrayImpl<T> {
 protected:
  T *data_ = nullptr;

 public:
  VMutableArrayImpl_For_MutableSpan(const MutableSpan<T> data)
      : VMutableArrayImpl<T>(data.size()), data_(data.data())
  {
  }

 protected:
  VMutableArrayImpl_For_MutableSpan(const int64_t size) : VMutableArrayImpl<T>(size)
  {
  }

  T get_impl(const int64_t index) const final
  {
    return data_[index];
  }

  void set_impl(const int64_t index, T value) final
  {
    data_[index] = value;
  }

  bool is_span_impl() const override
  {
    return true;
  }

  Span<T> get_internal_span_impl() const override
  {
    return Span<T>(data_, this->size_);
  }
};

/**
 * A variant of `VArrayImpl_For_Span` that owns the underlying data.
 * The `Container` type has to implement a `size()` and `data()` method.
 * The `data()` method has to return a pointer to the first element in the continuous array of
 * elements.
 */
template<typename Container, typename T = typename Container::value_type>
class VArrayImpl_For_ArrayContainer : public VArrayImpl_For_Span<T> {
 private:
  Container container_;

 public:
  VArrayImpl_For_ArrayContainer(Container container)
      : VArrayImpl_For_Span<T>((int64_t)container.size()), container_(std::move(container))
  {
    this->data_ = container_.data();
  }
};

/**
 * A virtual array implementation that returns the same value for every index. This class is final
 * so that it can be devirtualized by the compiler in some cases (e.g. when #devirtualize_varray is
 * used).
 */
template<typename T> class VArrayImpl_For_Single final : public VArrayImpl<T> {
 private:
  T value_;

 public:
  VArrayImpl_For_Single(T value, const int64_t size)
      : VArrayImpl<T>(size), value_(std::move(value))
  {
  }

 protected:
  T get_impl(const int64_t UNUSED(index)) const override
  {
    return value_;
  }

  bool is_span_impl() const override
  {
    return this->size_ == 1;
  }

  Span<T> get_internal_span_impl() const override
  {
    return Span<T>(&value_, 1);
  }

  bool is_single_impl() const override
  {
    return true;
  }

  T get_internal_single_impl() const override
  {
    return value_;
  }
};

/**
 * This class makes it easy to create a virtual array for an existing function or lambda. The
 * `GetFunc` should take a single `index` argument and return the value at that index.
 */
template<typename T, typename GetFunc> class VArrayImpl_For_Func final : public VArrayImpl<T> {
 private:
  GetFunc get_func_;

 public:
  VArrayImpl_For_Func(const int64_t size, GetFunc get_func)
      : VArrayImpl<T>(size), get_func_(std::move(get_func))
  {
  }

 private:
  T get_impl(const int64_t index) const override
  {
    return get_func_(index);
  }

  void materialize_impl(IndexMask mask, MutableSpan<T> r_span) const override
  {
    T *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { dst[i] = get_func_(i); });
  }

  void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<T> r_span) const override
  {
    T *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { new (dst + i) T(get_func_(i)); });
  }
};

template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class VArrayImpl_For_DerivedSpan : public VArrayImpl<ElemT> {
 private:
  const StructT *data_;

 public:
  VArrayImpl_For_DerivedSpan(const Span<StructT> data)
      : VArrayImpl<ElemT>(data.size()), data_(data.data())
  {
  }

 private:
  ElemT get_impl(const int64_t index) const override
  {
    return GetFunc(data_[index]);
  }

  void materialize_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { dst[i] = GetFunc(data_[i]); });
  }

  void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { new (dst + i) ElemT(GetFunc(data_[i])); });
  }
};

template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, ElemT)>
class VMutableArrayImpl_For_DerivedSpan : public VMutableArrayImpl<ElemT> {
 private:
  StructT *data_;

 public:
  VMutableArrayImpl_For_DerivedSpan(const MutableSpan<StructT> data)
      : VMutableArrayImpl<ElemT>(data.size()), data_(data.data())
  {
  }

 private:
  ElemT get_impl(const int64_t index) const override
  {
    return GetFunc(data_[index]);
  }

  void set_impl(const int64_t index, ElemT value) override
  {
    SetFunc(data_[index], std::move(value));
  }

  void materialize_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { dst[i] = GetFunc(data_[i]); });
  }

  void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { new (dst + i) ElemT(GetFunc(data_[i])); });
  }
};

/**
 * Generate multiple versions of the given function optimized for different virtual arrays.
 * One has to be careful with nesting multiple devirtualizations, because that results in an
 * exponential number of function instantiations (increasing compile time and binary size).
 *
 * Generally, this function should only be used when the virtual method call overhead to get an
 * element from a virtual array is significant.
 */
template<typename T, typename Func>
inline void devirtualize_varray(const VArrayImpl<T> &varray, const Func &func, bool enable = true)
{
  /* Support disabling the devirtualization to simplify benchmarking. */
  if (enable) {
    if (varray.is_single()) {
      /* `VArrayImpl_For_Single` can be used for devirtualization, because it is declared `final`.
       */
      const VArrayImpl_For_Single<T> varray_single{varray.get_internal_single(), varray.size()};
      func(varray_single);
      return;
    }
    if (varray.is_span()) {
      /* `VArrayImpl_For_Span` can be used for devirtualization, because it is declared `final`. */
      const VArrayImpl_For_Span<T> varray_span{varray.get_internal_span()};
      func(varray_span);
      return;
    }
  }
  func(varray);
}

/**
 * Same as `devirtualize_varray`, but devirtualizes two virtual arrays at the same time.
 * This is better than nesting two calls to `devirtualize_varray`, because it instantiates fewer
 * cases.
 */
template<typename T1, typename T2, typename Func>
inline void devirtualize_varray2(const VArrayImpl<T1> &varray1,
                                 const VArrayImpl<T2> &varray2,
                                 const Func &func,
                                 bool enable = true)
{
  /* Support disabling the devirtualization to simplify benchmarking. */
  if (enable) {
    const bool is_span1 = varray1.is_span();
    const bool is_span2 = varray2.is_span();
    const bool is_single1 = varray1.is_single();
    const bool is_single2 = varray2.is_single();
    if (is_span1 && is_span2) {
      const VArrayImpl_For_Span<T1> varray1_span{varray1.get_internal_span()};
      const VArrayImpl_For_Span<T2> varray2_span{varray2.get_internal_span()};
      func(varray1_span, varray2_span);
      return;
    }
    if (is_span1 && is_single2) {
      const VArrayImpl_For_Span<T1> varray1_span{varray1.get_internal_span()};
      const VArrayImpl_For_Single<T2> varray2_single{varray2.get_internal_single(),
                                                     varray2.size()};
      func(varray1_span, varray2_single);
      return;
    }
    if (is_single1 && is_span2) {
      const VArrayImpl_For_Single<T1> varray1_single{varray1.get_internal_single(),
                                                     varray1.size()};
      const VArrayImpl_For_Span<T2> varray2_span{varray2.get_internal_span()};
      func(varray1_single, varray2_span);
      return;
    }
    if (is_single1 && is_single2) {
      const VArrayImpl_For_Single<T1> varray1_single{varray1.get_internal_single(),
                                                     varray1.size()};
      const VArrayImpl_For_Single<T2> varray2_single{varray2.get_internal_single(),
                                                     varray2.size()};
      func(varray1_single, varray2_single);
      return;
    }
  }
  /* This fallback is used even when one of the inputs could be optimized. It's probably not worth
   * it to optimize just one of the inputs, because then the compiler still has to call into
   * unknown code, which inhibits many compiler optimizations. */
  func(varray1, varray2);
}

namespace detail {

template<typename T> struct VArrayAnyExtraInfo {
  const VArrayImpl<T> *(*get_varray)(const void *buffer) =
      [](const void *UNUSED(buffer)) -> const VArrayImpl<T> * { return nullptr; };

  template<typename StorageT> static VArrayAnyExtraInfo get()
  {
    static_assert(std::is_base_of_v<VArrayImpl<T>, StorageT> ||
                  std::is_same_v<StorageT, const VArrayImpl<T> *> ||
                  std::is_same_v<StorageT, std::shared_ptr<const VArrayImpl<T>>>);

    if constexpr (std::is_base_of_v<VArrayImpl<T>, StorageT>) {
      return {[](const void *buffer) {
        return static_cast<const VArrayImpl<T> *>((const StorageT *)buffer);
      }};
    }
    else if constexpr (std::is_same_v<StorageT, const VArrayImpl<T> *>) {
      return {[](const void *buffer) { return *(const StorageT *)buffer; }};
    }
    else if constexpr (std::is_same_v<StorageT, std::shared_ptr<const VArrayImpl<T>>>) {
      return {[](const void *buffer) { return ((const StorageT *)buffer)->get(); }};
    }
    else {
      BLI_assert_unreachable();
      return {};
    }
  }
};

}  // namespace detail

template<typename T> class VMutableArray;

template<typename T> class VArray {
 private:
  using ExtraInfo = detail::VArrayAnyExtraInfo<T>;
  using Storage = Any<ExtraInfo, 24, 8>;
  using Impl = VArrayImpl<T>;

  const Impl *impl_ = nullptr;
  Storage storage_;

  friend class VMutableArray<T>;

 public:
  VArray() = default;

  VArray(const VArray &other) : storage_(other.storage_)
  {
    impl_ = storage_.extra_info().get_varray(storage_.get());
  }

  VArray(const Impl *impl) : impl_(impl)
  {
    storage_ = impl_;
  }

  VArray(std::shared_ptr<const Impl> impl) : impl_(impl.get())
  {
    if (impl) {
      storage_ = std::move(impl);
    }
  }

  template<typename ImplT, typename... Args> static VArray For(Args &&...args)
  {
    static_assert(std::is_base_of_v<Impl, ImplT>);
    if constexpr (std::is_copy_constructible_v<ImplT> && Storage::template is_inline_v<ImplT>) {
      VArray varray;
      varray.impl_ = &varray.storage_.template emplace<ImplT>(std::forward<Args>(args)...);
      return varray;
    }
    else {
      return VArray(std::make_shared<ImplT>(std::forward<Args>(args)...));
    }
  }

  static VArray ForSingle(T value, const int64_t size)
  {
    return VArray::For<VArrayImpl_For_Single<T>>(std::move(value), size);
  }

  static VArray ForSpan(Span<T> values)
  {
    return VArray::For<VArrayImpl_For_Span<T>>(values);
  }

  template<typename GetFunc> static VArray ForFunc(const int64_t size, GetFunc get_func)
  {
    return VArray::For<VArrayImpl_For_Func<T, decltype(get_func)>>(size, std::move(get_func));
  }

  template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
  static VArray ForDerivedSpan(Span<StructT> values)
  {
    return VArray::For<VArrayImpl_For_DerivedSpan<StructT, ElemT, GetFunc>>(values);
  }

  template<typename ContainerT> static VArray ForContainer(ContainerT container)
  {
    return VArray::For<VArrayImpl_For_ArrayContainer<ContainerT>>(std::move(container));
  }

  operator bool() const
  {
    return impl_ != nullptr;
  }

  VArray &operator=(const VArray &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~VArray();
    new (this) VArray(other);
    return *this;
  }

  VArray &operator=(VArray &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~VArray();
    new (this) VArray(std::move(other));
    return *this;
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

  T operator[](const int64_t index) const
  {
    BLI_assert(*this);
    return impl_->get(index);
  }
};

template<typename T> class VMutableArray {
 private:
  using ExtraInfo = detail::VArrayAnyExtraInfo<T>;
  using Storage = Any<ExtraInfo, 24, 8>;
  using Impl = VMutableArrayImpl<T>;

  Impl *impl_ = nullptr;
  Storage storage_;

 public:
  VMutableArray() = default;

  VMutableArray(const VMutableArray &other) : storage_(other.storage_)
  {
    impl_ = const_cast<Impl *>(
        static_cast<const Impl *>(storage_.extra_info().get_varray(storage_.get())));
  }

  VMutableArray(Impl *impl) : impl_(impl)
  {
    storage_ = static_cast<const VArrayImpl<T> *>(impl);
  }

  VMutableArray(std::shared_ptr<Impl> impl) : impl_(impl.get())
  {
    if (impl) {
      storage_ = std::shared_ptr<const VArrayImpl<T>>(std::move(impl));
    }
  }

  template<typename ImplT, typename... Args> static VMutableArray For(Args &&...args)
  {
    static_assert(std::is_base_of_v<Impl, ImplT>);
    if constexpr (std::is_copy_constructible_v<ImplT> && Storage::template is_inline_v<ImplT>) {
      VMutableArray varray;
      varray.impl_ = &varray.storage_.template emplace<ImplT>(std::forward<Args>(args)...);
      return varray;
    }
    else {
      return VMutableArray(std::make_shared<ImplT>(std::forward<Args>(args)...));
    }
  }

  static VMutableArray ForSpan(MutableSpan<T> values)
  {
    return VMutableArray::For<VMutableArrayImpl_For_MutableSpan<T>>(values);
  }

  template<typename StructT,
           typename ElemT,
           ElemT (*GetFunc)(const StructT &),
           void (*SetFunc)(StructT &, ElemT)>
  static VMutableArray ForDerivedSpan(MutableSpan<StructT> values)
  {
    return VMutableArray::For<VMutableArrayImpl_For_DerivedSpan<StructT, ElemT, GetFunc, SetFunc>>(
        values);
  }

  operator bool() const
  {
    return impl_ != nullptr;
  }

  operator VArray<T>() const
  {
    VArray<T> varray;
    varray.storage_ = storage_;
    varray.impl_ = impl_;
    return varray;
  }

  VMutableArray &operator=(const VMutableArray &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~VMutableArray();
    new (this) VMutableArray(other);
    return *this;
  }

  VMutableArray &operator=(VMutableArray &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~VMutableArray();
    new (this) VMutableArray(std::move(other));
    return *this;
  }

  Impl *operator->() const
  {
    BLI_assert(*this);
    return impl_;
  }

  Impl &operator*() const
  {
    BLI_assert(*this);
    return *impl_;
  }

  T operator[](const int64_t index) const
  {
    BLI_assert(*this);
    return impl_->get(index);
  }
};

/**
 * In many cases a virtual array is a span internally. In those cases, access to individual could
 * be much more efficient than calling a virtual method. When the underlying virtual array is not a
 * span, this class allocates a new array and copies the values over.
 *
 * This should be used in those cases:
 *  - All elements in the virtual array are accessed multiple times.
 *  - In most cases, the underlying virtual array is a span, so no copy is necessary to benefit
 *    from faster access.
 *  - An API is called, that does not accept virtual arrays, but only spans.
 */
template<typename T> class VArray_Span final : public Span<T> {
 private:
  VArray<T> varray_;
  Array<T> owned_data_;

 public:
  VArray_Span(VArray<T> varray) : Span<T>(), varray_(std::move(varray))
  {
    this->size_ = varray_->size();
    if (varray_->is_span()) {
      this->data_ = varray_->get_internal_span().data();
    }
    else {
      owned_data_.~Array();
      new (&owned_data_) Array<T>(varray_->size(), NoInitialization{});
      varray_->materialize_to_uninitialized(owned_data_);
      this->data_ = owned_data_.data();
    }
  }
};

/**
 * Same as VArray_Span, but for a mutable span.
 * The important thing to note is that when changing this span, the results might not be
 * immediately reflected in the underlying virtual array (only when the virtual array is a span
 * internally). The #save method can be used to write all changes to the underlying virtual array,
 * if necessary.
 */
template<typename T> class VMutableArray_Span final : public MutableSpan<T> {
 private:
  VMutableArray<T> varray_;
  Array<T> owned_data_;
  bool save_has_been_called_ = false;
  bool show_not_saved_warning_ = true;

 public:
  /* Create a span for any virtual array. This is cheap when the virtual array is a span itself. If
   * not, a new array has to be allocated as a wrapper for the underlying virtual array. */
  VMutableArray_Span(VMutableArray<T> varray, const bool copy_values_to_span = true)
      : MutableSpan<T>(), varray_(std::move(varray))
  {
    this->size_ = varray_->size();
    if (varray_->is_span()) {
      this->data_ = varray_->get_internal_span().data();
    }
    else {
      if (copy_values_to_span) {
        owned_data_.~Array();
        new (&owned_data_) Array<T>(varray_->size(), NoInitialization{});
        varray_->materialize_to_uninitialized(owned_data_);
      }
      else {
        owned_data_.reinitialize(varray_->size());
      }
      this->data_ = owned_data_.data();
    }
  }

  ~VMutableArray_Span()
  {
    if (show_not_saved_warning_) {
      if (!save_has_been_called_) {
        std::cout << "Warning: Call `save()` to make sure that changes persist in all cases.\n";
      }
    }
  }

  /* Write back all values from a temporary allocated array to the underlying virtual array. */
  void save()
  {
    save_has_been_called_ = true;
    if (this->data_ != owned_data_.data()) {
      return;
    }
    varray_->set_all(owned_data_);
  }

  void disable_not_applied_warning()
  {
    show_not_saved_warning_ = false;
  }
};

}  // namespace blender
