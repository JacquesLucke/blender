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

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"

#include "FN_generic_virtual_vector_array.hh"

namespace blender::fn {

class GVectorArray : NonCopyable, NonMovable {
 private:
  struct Item {
    void *start = nullptr;
    int64_t length = 0;
    int64_t capacity = 0;
  };

  LinearAllocator<> allocator_;
  const CPPType &type_;
  const int64_t element_size_;
  Array<Item, 1> items_;

 public:
  GVectorArray() = delete;

  GVectorArray(const CPPType &type, int64_t array_size);

  ~GVectorArray();

  int64_t size() const
  {
    return items_.size();
  }

  bool is_empty() const
  {
    return items_.is_empty();
  }

  const CPPType &type() const
  {
    return type_;
  }

  void append(int64_t index, const void *value);

  void extend(int64_t index, const GVArray &values);
  void extend(int64_t index, GSpan values);

  GMutableSpan operator[](int64_t index);
  GSpan operator[](int64_t index) const;

  template<typename T> void append(const int64_t index, const T &value)
  {
    BLI_assert(type_.is<T>());
    this->append(index, &value);
  }

  template<typename T> void extend(const int64_t index, const VArray<T> &values)
  {
    BLI_assert(type_.is<T>());
    GVArrayForVArray<T> array{values};
    this->extend(index, array);
  }

  template<typename T> void extend(const int64_t index, Span<T> values)
  {
    BLI_assert(type_.is<T>());
    this->extend(index, values);
  }

 private:
  void realloc_to_at_least(Item &item, int64_t min_capacity);
};

class GVVectorArrayForGVectorArray : public GVVectorArray {
 private:
  const GVectorArray &vector_array_;

 public:
  GVVectorArrayForGVectorArray(const GVectorArray &vector_array)
      : GVVectorArray(vector_array.type(), vector_array.size()), vector_array_(vector_array)
  {
  }

 protected:
  int64_t get_vector_size_impl(const int64_t index) const override
  {
    return vector_array_[index].size();
  }

  void get_vector_element_impl(const int64_t index,
                               const int64_t index_in_vector,
                               void *r_value) const override
  {
    type_->copy_to_initialized(vector_array_[index][index_in_vector], r_value);
  }
};

}  // namespace blender::fn
