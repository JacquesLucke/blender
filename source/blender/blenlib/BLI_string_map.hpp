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

/** \file
 * \ingroup bli
 *
 * This tries to solve the issue that a normal map with std::string as key might do many
 * allocations when the keys are longer than 16 bytes (the usual small string optimization size).
 *
 * For now this still uses std::string, but having this abstraction in place will make it easier to
 * make it more efficient later on. Also, even if we will never implement this optimization, having
 * a special map with string keys can be quite handy. */

#pragma once

#include "BLI_small_map.hpp"
#include "BLI_string_ref.hpp"

namespace BLI {

template<typename V> class StringMap {
 private:
  SmallMap<std::string, V> m_map;

 public:
  StringMap() = default;

  void add_new(StringRef key, const V &value)
  {
    m_map.add_new(key.to_std_string(), value);
  }

  void add_override(StringRef key, const V &value)
  {
    m_map.add_override(key.to_std_string(), value);
  }

  V lookup(StringRef key) const
  {
    return m_map.lookup(key.to_std_string());
  }

  V *lookup_ptr(StringRef key) const
  {
    return m_map.lookup_ptr(key.to_std_string());
  }

  V lookup_default(StringRef key, const V &default_value) const
  {
    return m_map.lookup_default(key.to_std_string(), default_value);
  }

  V &lookup_ref(StringRef key) const
  {
    return m_map.lookup_ref(key.to_std_string());
  }

  bool contains(StringRef key) const
  {
    return m_map.contains(key.to_std_string());
  }

  decltype(m_map.items()) items() const
  {
    return m_map.items();
  }

  decltype(m_map.keys()) keys() const
  {
    return m_map.keys();
  }

  decltype(m_map.values()) values() const
  {
    return m_map.values();
  }
};

}  // namespace BLI
