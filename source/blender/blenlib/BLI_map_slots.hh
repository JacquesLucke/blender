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

#ifndef __BLI_MAP_SLOTS_HH__
#define __BLI_MAP_SLOTS_HH__

/** \file
 * \ingroup bli
 */

#include "BLI_memory_utils.hh"

namespace BLI {

template<typename Key, typename Value> class SimpleMapSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_occupied = 1;
  static constexpr uint8_t s_is_removed = 2;

  uint8_t m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_key_buffer;
  AlignedBuffer<sizeof(Value), alignof(Value)> m_value_buffer;

 public:
  SimpleMapSlot()
  {
    m_state = s_is_empty;
  }

  ~SimpleMapSlot()
  {
    if (m_state == s_is_occupied) {
      this->key()->~Key();
      this->value()->~Value();
    }
  }

  SimpleMapSlot(const SimpleMapSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_occupied) {
      new (this->key()) Key(*other.key());
      new (this->value()) Value(*other.value());
    }
  }

  SimpleMapSlot(SimpleMapSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_occupied) {
      new (this->key()) Key(std::move(*other.key()));
      new (this->value()) Value(std::move(*other.value()));
    }
  }

  Key *key()
  {
    return (Key *)m_key_buffer.ptr();
  }

  const Key *key() const
  {
    return (const Key *)m_key_buffer.ptr();
  }

  Value *value()
  {
    return (Value *)m_value_buffer.ptr();
  }

  const Value *value() const
  {
    return (const Value *)m_value_buffer.ptr();
  }

  bool is_occupied() const
  {
    return m_state == s_is_occupied;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  void relocate_occupied_here(SimpleMapSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = s_is_occupied;
    new (this->key()) Key(std::move(*other.key()));
    new (this->value()) Value(std::move(*other.value()));
    other.key()->~Key();
    other.value()->~Value();
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash)
  {
    return hash(*this->key());
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t UNUSED(hash)) const
  {
    if (m_state == s_is_occupied) {
      return key == *this->key();
    }
    return false;
  }

  template<typename ForwardKey, typename ForwardValue>
  void occupy(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    this->occupy_without_value(std::forward<ForwardKey>(key), hash);
    new (this->value()) Value(std::forward<ForwardValue>(value));
  }

  template<typename ForwardKey> void occupy_without_value(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    m_state = s_is_occupied;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = s_is_removed;
    this->key()->~Key();
    this->value()->~Value();
  }
};

template<typename Key, typename Value> struct DefaultMapSlot {
  using type = SimpleMapSlot<Key, Value>;
};

}  // namespace BLI

#endif /* __BLI_MAP_SLOTS_HH__ */
