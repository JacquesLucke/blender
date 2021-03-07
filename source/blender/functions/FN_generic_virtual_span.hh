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

}  // namespace blender::fn
