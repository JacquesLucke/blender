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

#include "BLI_virtual_span.hh"

namespace blender {

template<typename T> class VArraySpan {
 protected:
  int64_t size_ = 0;

 public:
  VArraySpan(const int64_t size = 0) : size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~VArraySpan() = default;

  int64_t size() const
  {
    return size_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  int64_t get_array_size(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_array_size_impl(index);
  }

  T get_array_element(const int64_t index, const int64_t index_in_array) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    BLI_assert(index < this->get_array_size(index));
    return this->get_array_element_impl(index, index_in_array);
  }

 protected:
  virtual int64_t get_array_size_impl(const int64_t index) const = 0;
  virtual T get_array_element_impl(const int64_t index, const int64_t index_in_array) const = 0;
};

template<typename T> class VSpanForVArraySpan final : public VSpan<T> {
 private:
  const VArraySpan<T> &array_span_;
  const int64_t index_;

 public:
  VSpanForVArraySpan(const VArraySpan<T> &array_span, const int64_t index)
      : VSpan<T>(array_span.get_array_size(index)), array_span_(array_span), index_(index)
  {
  }

 private:
  T get_element_impl(const int64_t index_in_array) const final
  {
    return array_span_.get_array_element(index, index_in_array);
  }
};

}  // namespace  blender
