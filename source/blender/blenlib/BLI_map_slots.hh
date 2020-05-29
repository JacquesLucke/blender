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
 *
 * This file contains slot types that are supposed to be used with BLI::Map.
 *
 * Every slot type has to be able to hold a value of type Key, a value of type Value and state
 * information. A map slot has three possible states: Empty, Occupied and Removed.
 *
 * Only when a slot is occupied, it stores instances of type Key and Value.
 *
 * A map slot type has to implement a couple of methods that are explained in SimpleMap Slot.
 * A slot type is assumed to be trivially destructable, when it is not in occupied state. So the
 * destructor might not be called in that case.
 */

#include "BLI_memory_utils.hh"

namespace BLI {

/**
 * The simplest possible map slot. It stores the slot state and the optional key and value
 * instances in separate variables. Depending on the alignment requirement of the key and value,
 * many bytes might be wasted.
 */
template<typename Key, typename Value> class SimpleMapSlot {
 private:
  enum State : uint8_t {
    Empty = 0,
    Occupied = 1,
    Removed = 2,
  };

  State m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_key_buffer;
  AlignedBuffer<sizeof(Value), alignof(Value)> m_value_buffer;

 public:
  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  SimpleMapSlot()
  {
    m_state = Empty;
  }

  /**
   * The destructor also has to destruct the key and value, if the slot is currently occupied.
   */
  ~SimpleMapSlot()
  {
    if (m_state == Occupied) {
      this->key()->~Key();
      this->value()->~Value();
    }
  }

  /**
   * The copy constructor has to copy the state. If the other slot was occupied, a copy of the key
   * and value have to be made as well.
   */
  SimpleMapSlot(const SimpleMapSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new (this->key()) Key(*other.key());
      new (this->value()) Value(*other.value());
    }
  }

  /**
   * The move construtor has to copy the state. If the other slot was occupied, the key and value
   * from the other have to moved as well. The other slot stays in the state it was in before. Its
   * optionally stored key and value remain in a moved-from state.
   */
  SimpleMapSlot(SimpleMapSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new (this->key()) Key(std::move(*other.key()));
      new (this->value()) Value(std::move(*other.value()));
    }
  }

  /**
   * Get a non-const pointer to the position where the key is stored.
   */
  Key *key()
  {
    return (Key *)m_key_buffer.ptr();
  }

  /**
   * Get a const pointer to the position where the key is stored.
   */
  const Key *key() const
  {
    return (const Key *)m_key_buffer.ptr();
  }

  /**
   * Get a non-const pointer to the position where the value is stored.
   */
  Value *value()
  {
    return (Value *)m_value_buffer.ptr();
  }

  /**
   * Get a const pointer to the position where the value is stored.
   */
  const Value *value() const
  {
    return (const Value *)m_value_buffer.ptr();
  }

  /**
   * Return true if the slot currently contains a key and a value.
   */
  bool is_occupied() const
  {
    return m_state == Occupied;
  }

  /**
   * Return true if the slot is empty, i.e. it does not contain a key and is not in removed state.
   */
  bool is_empty() const
  {
    return m_state == Empty;
  }

  /**
   * Return the hash of the currently stored key. The hash function is guaranteed to stay the same
   * during the lifetime of a slot. In this simple map slot implementation, we just computed the
   * hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint32_t get_hash(const Hash &hash)
  {
    return hash(*this->key());
  }

  /**
   * Move the other slot into this slot and destruct it. We do destruction here, because this way
   * we can avoid a comparison with the state, since we know the slot is occupied.
   */
  void relocate_occupied_here(SimpleMapSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = Occupied;
    new (this->key()) Key(std::move(*other.key()));
    new (this->value()) Value(std::move(*other.value()));
    other.key()->~Key();
    other.value()->~Value();
  }

  /**
   * Return true, when this slot is occupied and contains a key that compares equal to the given
   * key. The hash can be used by other slot implementations to determine inequality faster.
   */
  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t UNUSED(hash)) const
  {
    if (m_state == Occupied) {
      return key == *this->key();
    }
    return false;
  }

  /**
   * Change the state of this slot from empty/removed to occupied. The key/value has to be
   * constructed by calling the constructor with the given key/value as parameter.
   */
  template<typename ForwardKey, typename ForwardValue>
  void occupy(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    this->occupy_without_value(std::forward<ForwardKey>(key), hash);
    new (this->value()) Value(std::forward<ForwardValue>(value));
  }

  /**
   * Change the state of this slot from empty/removed to occupied, but leave the value
   * uninitialized. The caller is responsible to construct the value afterwards.
   */
  template<typename ForwardKey> void occupy_without_value(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    m_state = Occupied;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  /**
   * Change the state of this slot from occupied to removed. The key and value have to be
   * destructed as well.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = Removed;
    this->key()->~Key();
    this->value()->~Value();
  }
};

template<typename Key, typename Value> struct DefaultMapSlot {
  using type = SimpleMapSlot<Key, Value>;
};

}  // namespace BLI

#endif /* __BLI_MAP_SLOTS_HH__ */
