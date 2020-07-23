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

#ifndef __BLI_MULTI_VALUE_MAP_HH__
#define __BLI_MULTI_VALUE_MAP_HH__

#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename Key, typename Value, typename Allocator = GuardedAllocator> class MultiValueMap {
 private:
  Map<Key, Vector<Value>> map_;

 public:
  void add(const Key &key, const Value &value)
  {
    this->add_as(key, value);
  }
  void add(const Key &key, Value &&value)
  {
    this->add_as(key, std::move(value));
  }
  void add(Key &&key, const Value &value)
  {
    this->add_as(std::move(key), value);
  }
  void add(Key &&key, Value &&value)
  {
    this->add_as(std::move(key), std::move(value));
  }
  template<typename ForwardKey, typename ForwardValue>
  void add_as(ForwardKey &&key, ForwardValue &&value)
  {
    Vector<Value> &vector = map_.lookup_or_add_default_as(std::forward<ForwardKey>(key));
    vector.append(std::forward<ForwardValue>(value));
  }

  void add_multiple(const Key &key, Span<Value> values)
  {
    this->add_multiple_as(key, values);
  }
  void add_multiple(Key &&key, Span<Value> values)
  {
    this->add_multiple_as(std::move(key), values);
  }
  template<typename ForwardKey> void add_multiple_as(ForwardKey &&key, Span<Value> values)
  {
    Vector<Value> &vector = map_.lookup_or_add_default_as(std::forward<ForwardKey>(key));
    vector.extend(values);
  }

  Span<Value> lookup(const Key &key) const
  {
    return this->lookup_as(key);
  }
  template<typename ForwardKey> Span<Value> lookup_as(const ForwardKey &key) const
  {
    const Vector<Value> *vector = map_.lookup_ptr_as(key);
    if (vector != nullptr) {
      return vector->as_span();
    }
    return {};
  }

  auto items() const
  {
    return map_.items();
  }

  auto values() const
  {
    return map_.values();
  }
};

}  // namespace blender

#endif /* __BLI_MULTI_VALUE_MAP_HH__ */
