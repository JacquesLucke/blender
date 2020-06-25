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

#ifndef __FN_ATTRIBUTES_REF_HH__
#define __FN_ATTRIBUTES_REF_HH__

/** \file
 * \ingroup fn
 */

#include "FN_spans.hh"

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector_set.hh"

namespace blender {
namespace fn {

class AttributesInfo;

class AttributesInfoBuilder : NonCopyable, NonMovable {
 private:
  LinearAllocator<> m_allocator;
  VectorSet<std::string> m_names;
  Vector<const CPPType *> m_types;
  Vector<void *> m_defaults;

  friend AttributesInfo;

 public:
  AttributesInfoBuilder() = default;
  ~AttributesInfoBuilder();

  template<typename T> void add(StringRef name, const T &default_value)
  {
    this->add(name, CPPType::get<T>(), (const void *)&default_value);
  }

  void add(StringRef name, const CPPType &type, const void *default_value = nullptr)
  {
    if (m_names.add_as(name)) {
      m_types.append(&type);

      if (default_value == nullptr) {
        default_value = type.default_value();
      }
      void *dst = m_allocator.allocate(type.size(), type.alignment());
      type.copy_to_uninitialized(default_value, dst);
      m_defaults.append(dst);
    }
    else {
      /* The same name can be added more than once as long as the type is always the same. */
      BLI_assert(m_types[m_names.index_of_as(name)] == &type);
    }
  }
};

class AttributesInfo : NonCopyable, NonMovable {
 private:
  LinearAllocator<> m_allocator;
  Map<StringRefNull, uint> m_index_by_name;
  Vector<StringRefNull> m_name_by_index;
  Vector<const CPPType *> m_type_by_index;
  Vector<void *> m_defaults;

 public:
  AttributesInfo() = default;
  AttributesInfo(const AttributesInfoBuilder &builder);
  ~AttributesInfo();

  uint size() const
  {
    return m_name_by_index.size();
  }

  IndexRange index_range() const
  {
    return m_name_by_index.index_range();
  }

  StringRefNull name_of(uint index) const
  {
    return m_name_by_index[index];
  }

  uint index_of(StringRef name) const
  {
    return m_index_by_name.lookup_as(name);
  }

  const void *default_of(uint index) const
  {
    return m_defaults[index];
  }

  const void *default_of(StringRef name) const
  {
    return this->default_of(this->index_of(name));
  }

  const CPPType &type_of(uint index) const
  {
    return *m_type_by_index[index];
  }

  const CPPType &type_of(StringRef name) const
  {
    return this->type_of(this->index_of(name));
  }

  bool has_attribute(StringRef name, const CPPType &type) const
  {
  }

  int try_index_of(StringRef name) const
  {
    return (int)m_index_by_name.lookup_default_as(name, -1);
  }

  int try_index_of(StringRef name, const CPPType &type) const
  {
    int index = this->try_index_of(name);
    if (index == -1) {
      return -1;
    }
    else if (this->type_of((uint)index) == type) {
      return index;
    }
    else {
      return -1;
    }
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_ATTRIBUTES_REF_HH__ */
