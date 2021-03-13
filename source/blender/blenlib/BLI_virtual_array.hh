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

#include "BLI_span.hh"

namespace blender {

template<typename T> class VArray {
 protected:
  int64_t size_;

 public:
  VArray(const int64_t size) : size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~VArray() = default;

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

  bool is_span() const
  {
    if (size_ == 0) {
      return true;
    }
    return this->is_span_impl();
  }

  Span<T> get_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return {};
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

  T get_single() const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      return this->get(0);
    }
    return this->get_single_impl();
  }

 protected:
  virtual T get_impl(const int64_t index) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual Span<T> get_span_impl() const
  {
    BLI_assert(false);
    return {};
  }

  virtual bool is_single_impl() const
  {
    return false;
  }

  virtual T get_single_impl() const
  {
    BLI_assert(false);
    return T();
  }
};

template<typename T> class VMutableArray : public VArray<T> {
 public:
  VMutableArray(const int64_t size) : VArray<T>(size)
  {
  }

  void set(const int64_t index, const T &value)
  {
    this->set_impl(index, value);
  }

  void set(const int64_t index, T &&value)
  {
    this->set_impl(index, std::move(value));
  }

  MutableSpan<T> get_span()
  {
    BLI_assert(this->is_span());
    if (this->size_ == 0) {
      return {};
    }
    return this->get_span_impl();
  }

 protected:
  virtual void set_impl(const int64_t index, const T &value) = 0;

  virtual void set_impl(const int64_t index, T &&value)
  {
    this->set_impl(index, value);
  }

  virtual MutableSpan<T> get_span_impl()
  {
    BLI_assert(false);
    return {};
  }
};

template<typename T> class VArrayForSpan : public VArray<T> {
 private:
  const T *data_;

 public:
  VArrayForSpan(const Span<T> data) : VArray<T>(data.size()), data_(data.data())
  {
  }

 protected:
  T get_impl(const int64_t index) const override
  {
    return data_[index];
  }

  bool is_span_impl() const override
  {
    return true;
  }

  Span<T> get_span_impl() const override
  {
    return Span<T>(data_, this->size_);
  }
};

template<typename T> class VMutableArrayForMutableSpan : public VMutableArray<T> {
 private:
  T *data_;

 public:
  VMutableArrayForMutableSpan(const MutableSpan<T> data)
      : VMutableArray<T>(data.size()), data_(data.data())
  {
  }

 protected:
  T get_impl(const int64_t index) const override
  {
    return data_[index];
  }

  void set_impl(const int64_t index, const T &value) override
  {
    data_[index] = value;
  }

  void set_impl(const int64_t index, T &&value) override
  {
    data_[index] = std::move(value);
  }

  bool is_span_impl() const override
  {
    return true;
  }

  Span<T> get_span_impl() const override
  {
    return Span<T>(data_, this->size_);
  }

  MutableSpan<T> get_span_impl() override
  {
    return MutableSpan<T>(data_, this->size_);
  }
};

template<typename T> class VArrayForSingle : public VArray<T> {
 private:
  T value_;

 public:
  VArrayForSingle(T value, const int64_t size) : VArray<T>(size), value_(std::move(value))
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

  Span<T> get_span_impl() const override
  {
    return Span<T>(&value_, 1);
  }

  bool is_single_impl() const override
  {
    return true;
  }

  T get_single_impl() const override
  {
    return value_;
  }
};

}  // namespace blender
