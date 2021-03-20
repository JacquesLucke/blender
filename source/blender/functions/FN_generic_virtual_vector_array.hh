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

#include "FN_generic_virtual_array.hh"

namespace blender::fn {

class GVVectorArray {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVVectorArray(const CPPType &type, const int64_t size) : type_(&type), size_(size)
  {
  }

  virtual ~GVVectorArray() = default;

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

  int64_t get_vector_size(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_vector_size_impl(index);
  }

  void get_vector_element(const int64_t index, const int64_t index_in_vector, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    BLI_assert(index_in_vector >= 0);
    BLI_assert(index_in_vector < this->get_vector_size(index));
    this->get_vector_element_impl(index, index_in_vector, r_value);
  }

  bool is_single_vector() const
  {
    if (size_ == 1) {
      return true;
    }
    return this->is_single_vector_impl();
  }

 protected:
  virtual int64_t get_vector_size_impl(const int64_t index) const = 0;

  virtual void get_vector_element_impl(const int64_t index,
                                       const int64_t index_in_vector,
                                       void *r_value) const = 0;

  virtual bool is_single_vector_impl() const
  {
    return false;
  }
};

}  // namespace blender::fn
