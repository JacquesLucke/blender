/* Apache License, Version 2.0 */

#include "BLI_array.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"
#include "BLI_virtual_array.hh"
#include "testing/testing.h"

#include <optional>

namespace blender {

struct AnyInfo {
  bool is_unique_ptr;
  void (*copy_construct)(void *dst, const void *src);
  void (*move_construct)(void *dst, void *src);
  void (*copy_assign)(void *dst, const void *src);
  void (*move_assign)(void *dst, void *src);
  void (*destruct)(void *src);
  const void *(*get)(const void *src);

  template<typename T> static const AnyInfo &get_for_inline()
  {
    static AnyInfo funcs = {false,
                            [](void *dst, const void *src) { new (dst) T(*(const T *)src); },
                            [](void *dst, void *src) { new (dst) T(std::move(*(T *)src)); },
                            [](void *dst, const void *src) { *(T *)dst = *(const T *)src; },
                            [](void *dst, void *src) { *(T *)dst = std::move(*(T *)src); },
                            [](void *src) { ((T *)src)->~T(); },
                            [](const void *src) { return src; }};
    return funcs;
  }

  template<typename T> static const AnyInfo &get_for_unique_ptr()
  {
    using Ptr = std::unique_ptr<T>;
    static AnyInfo funcs = {
        true,
        [](void *dst, const void *src) { new (dst) Ptr(new T(**(const Ptr *)src)); },
        [](void *dst, void *src) { new (dst) Ptr(new T(std::move(**(Ptr *)src))); },
        [](void *dst, const void *src) { *(Ptr *)dst = Ptr(new T(**(const Ptr *)src)); },
        [](void *dst, void *src) { *(Ptr *)dst = Ptr(new T(std::move(**(Ptr *)src))); },
        [](void *src) { ((Ptr *)src)->~Ptr(); },
        [](const void *src) -> const void * { return &**(const Ptr *)src; }};
    return funcs;
  }

  static const AnyInfo &get_for_empty()
  {
    static AnyInfo funcs = {false,
                            [](void *UNUSED(dst), const void *UNUSED(src)) {},
                            [](void *UNUSED(dst), void *UNUSED(src)) {},
                            [](void *UNUSED(dst), const void *UNUSED(src)) {},
                            [](void *UNUSED(dst), void *UNUSED(src)) {},
                            [](void *UNUSED(src)) {},
                            [](const void *UNUSED(src)) -> const void * { return nullptr; }};
    return funcs;
  }
};

template<int64_t InlineBufferCapacity = 16, int64_t Alignment = 8> class Any {
 private:
  AlignedBuffer<std::max((size_t)InlineBufferCapacity, sizeof(std::unique_ptr<int>)),
                (size_t)Alignment>
      buffer_;
  const AnyInfo *info_ = &AnyInfo::get_for_empty();

 public:
  /* TODO: Check nothrow movability. */
  template<typename T>
  static constexpr inline bool is_inline_v = sizeof(T) <= InlineBufferCapacity &&
                                             alignof(T) <= Alignment;

  Any() = default;

  Any(const Any &other)
  {
    info_ = other.info_;
    info_->copy_construct(&buffer_, &other.buffer_);
  }

  Any(Any &&other)
  {
    info_ = other.info_;
    info_->move_construct(&buffer_, &other.buffer_);
  }

  /* TODO: bugprone-forwarding-reference-overload. */
  template<typename T,
           typename std::enable_if_t<!std::is_same_v<std::decay_t<T>, Any>> * = nullptr>
  Any(T &&value)
  {
    using DecayT = std::decay_t<T>;
    if constexpr (is_inline_v<T>) {
      info_ = &AnyInfo::get_for_inline<DecayT>();
      new (&buffer_) T(std::forward<T>(value));
    }
    else {
      info_ = &AnyInfo::get_for_unique_ptr<DecayT>();
      new (&buffer_) std::unique_ptr<DecayT>(new DecayT(std::forward<T>(value)));
    }
  }

  ~Any()
  {
    this->reset();
  }

  Any &operator=(const Any &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~Any();
    new (this) Any(other);
    return *this;
  }

  Any &operator=(Any &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~Any();
    new (this) Any(std::move(other));
    return *this;
  }

  template<typename T> Any operator=(T &&other)
  {
    this->~Any();
    new (this) Any(std::forward<T>(other));
    return *this;
  }

  template<typename T> T &get()
  {
    return *(T *)info_->get(&buffer_);
  }

  template<typename T> const T &get() const
  {
    return *(const T *)info_->get(&buffer_);
  }

  void reset()
  {
    info_->destruct(buffer_);
    info_ = &AnyInfo::get_for_empty();
  }
};

template<typename T> struct VArrayValueImpls {
  /* TODO: Use #std::variant. */
  std::optional<VArray_For_Span<T>> varray_span;
  std::optional<VArray_For_Single<T>> varray_single;
  std::shared_ptr<const VArray<T>> varray_any;

  const VArray<T> *get() const
  {
    if (this->varray_span.has_value()) {
      return &*this->varray_span;
    }
    if (this->varray_single.has_value()) {
      return &*this->varray_single;
    }
    return varray_any.get();
  }

  void reset()
  {
    varray_span.reset();
    varray_single.reset();
    varray_any.reset();
  }
};

template<typename T> class VArrayValue {
 private:
  VArrayValueImpls<T> impls_;
  const VArray<T> *varray_ = nullptr;

 public:
  VArrayValue() = default;

  VArrayValue(const VArrayValue &other) : impls_(other.impls_), varray_(impls_.get())
  {
  }

  VArrayValue(VArrayValue &&other) : impls_(std::move(other.impls_)), varray_(impls_.get())
  {
    other.varray_ = nullptr;
    other.impls_.reset();
  }

  VArrayValue &operator=(const VArrayValue &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~VArrayValue();
    new (this) VArrayValue(other);
    return *this;
  }

  VArrayValue &operator=(VArrayValue &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~VArrayValue();
    new (this) VArrayValue(std::move(other));
    return *this;
  }

  VArrayValue(Span<T> values)
  {
    impls_.varray_span.emplace(values);
    varray_ = &*impls_.varray_span;
  }

  VArrayValue(T value, int64_t size)
  {
    impls_.varray_single.emplace(std::move(value), size);
    varray_ = &*impls_.varray_single;
  }

  VArrayValue(std::shared_ptr<const VArray<T>> varray)
  {
    impls_.varray_any = std::move(varray);
    varray_ = &*impls_.varray_any.get();
  }

  operator bool() const
  {
    return varray_ != nullptr;
  }

  const VArray<T> *operator->()
  {
    BLI_assert(varray_ != nullptr);
    return varray_;
  }

  const VArray<T> &operator*() const
  {
    BLI_assert(varray_ != nullptr);
    return *varray_;
  }

  T operator[](const int64_t index) const
  {
    BLI_assert(varray_ != nullptr);
    return varray_->get(index);
  }
};
}  // namespace blender

namespace blender::tests {

TEST(virtual_array, Span)
{
  std::array<int, 5> data = {3, 4, 5, 6, 7};
  VArray_For_Span<int> varray{data};
  EXPECT_EQ(varray.size(), 5);
  EXPECT_EQ(varray.get(0), 3);
  EXPECT_EQ(varray.get(4), 7);
  EXPECT_TRUE(varray.is_span());
  EXPECT_FALSE(varray.is_single());
  EXPECT_EQ(varray.get_internal_span().data(), data.data());
}

TEST(virtual_array, Single)
{
  VArray_For_Single<int> varray{10, 4};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray.get(0), 10);
  EXPECT_EQ(varray.get(3), 10);
  EXPECT_FALSE(varray.is_span());
  EXPECT_TRUE(varray.is_single());
}

TEST(virtual_array, Array)
{
  Array<int> array = {1, 2, 3, 5, 8};
  {
    VArray_For_ArrayContainer varray{array};
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArray_For_ArrayContainer varray{std::move(array)};
    EXPECT_EQ(varray.size(), 5);
    EXPECT_EQ(varray[0], 1);
    EXPECT_EQ(varray[2], 3);
    EXPECT_EQ(varray[3], 5);
    EXPECT_TRUE(varray.is_span());
  }
  {
    VArray_For_ArrayContainer varray{array}; /* NOLINT: bugprone-use-after-move */
    EXPECT_TRUE(varray.is_empty());
  }
}

TEST(virtual_array, Vector)
{
  Vector<int> vector = {9, 8, 7, 6};
  VArray_For_ArrayContainer varray{std::move(vector)};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 9);
  EXPECT_EQ(varray[3], 6);
}

TEST(virtual_array, StdVector)
{
  std::vector<int> vector = {5, 6, 7, 8};
  VArray_For_ArrayContainer varray{std::move(vector)};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 6);
}

TEST(virtual_array, StdArray)
{
  std::array<int, 4> array = {2, 3, 4, 5};
  VArray_For_ArrayContainer varray{array};
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 2);
  EXPECT_EQ(varray[1], 3);
}

TEST(virtual_array, VectorSet)
{
  VectorSet<int> vector_set = {5, 3, 7, 3, 3, 5, 1};
  VArray_For_ArrayContainer varray{std::move(vector_set)};
  EXPECT_TRUE(vector_set.is_empty()); /* NOLINT: bugprone-use-after-move. */
  EXPECT_EQ(varray.size(), 4);
  EXPECT_EQ(varray[0], 5);
  EXPECT_EQ(varray[1], 3);
  EXPECT_EQ(varray[2], 7);
  EXPECT_EQ(varray[3], 1);
}

TEST(virtual_array, Func)
{
  auto func = [](int64_t index) { return (int)(index * index); };
  VArray_For_Func<int, decltype(func)> varray{10, func};
  EXPECT_EQ(varray.size(), 10);
  EXPECT_EQ(varray[0], 0);
  EXPECT_EQ(varray[3], 9);
  EXPECT_EQ(varray[9], 81);
}

TEST(virtual_array, AsSpan)
{
  auto func = [](int64_t index) { return (int)(10 * index); };
  VArray_For_Func<int, decltype(func)> func_varray{10, func};
  VArray_Span span_varray{func_varray};
  EXPECT_EQ(span_varray.size(), 10);
  Span<int> span = span_varray;
  EXPECT_EQ(span.size(), 10);
  EXPECT_EQ(span[0], 0);
  EXPECT_EQ(span[3], 30);
  EXPECT_EQ(span[6], 60);
}

static int get_x(const std::array<int, 3> &item)
{
  return item[0];
}

static void set_x(std::array<int, 3> &item, int value)
{
  item[0] = value;
}

TEST(virtual_array, DerivedSpan)
{
  Vector<std::array<int, 3>> vector;
  vector.append({3, 4, 5});
  vector.append({1, 1, 1});
  {
    VArray_For_DerivedSpan<std::array<int, 3>, int, get_x> varray{vector};
    EXPECT_EQ(varray.size(), 2);
    EXPECT_EQ(varray[0], 3);
    EXPECT_EQ(varray[1], 1);
  }
  {
    VMutableArray_For_DerivedSpan<std::array<int, 3>, int, get_x, set_x> varray{vector};
    EXPECT_EQ(varray.size(), 2);
    EXPECT_EQ(varray[0], 3);
    EXPECT_EQ(varray[1], 1);
    varray.set(0, 10);
    varray.set(1, 20);
    EXPECT_EQ(vector[0][0], 10);
    EXPECT_EQ(vector[1][0], 20);
  }
}

TEST(virtual_array_value, MyTest)
{
  VArrayValue<int> varray{10, 20};
  EXPECT_EQ(varray->size(), 20);
  EXPECT_EQ(varray[3], 10);

  Any my_any = Vector<int>();
  Any other = my_any;
}

}  // namespace blender::tests
