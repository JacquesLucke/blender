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

#ifndef __BLI_SET_SLOTS_HH__
#define __BLI_SET_SLOTS_HH__

/** \file
 * \ingroup bli
 *
 * This file contains different slot types that are supposed to be used with BLI::Set.
 *
 * Every slot type has to be able to hold a value of the Key type and state information.
 * A set slot has three possible states: Empty, Occupied and Removed.
 *
 * Only when a slot is occupied, it stores an instance of type Key.
 *
 * A set slot type has to implement a couple of methods that are explained in SimpleSetSlot.
 * A slot type is assumed to be trivially destructable, when it is not in occupied state. So the
 * destructor might not be called in that case.
 */

#include "BLI_memory_utils.hh"

namespace BLI {

/**
 * The simplest possible set slot. It stores the slot state and the optional key instance in
 * separate variables. Depending on the alignment requirement of the key, many bytes might be
 * wasted.
 */
template<typename Key> class SimpleSetSlot {
 private:
  enum State : uint8_t {
    Empty = 0,
    Occupied = 1,
    Removed = 2,
  };

  State m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  /**
   * After the default constructor has run, the slot has to be in the empty state.
   */
  SimpleSetSlot()
  {
    m_state = Empty;
  }

  /**
   * The destructor also has to destruct the key, if the slot is currently occupied.
   */
  ~SimpleSetSlot()
  {
    if (m_state == Occupied) {
      this->key()->~Key();
    }
  }

  /**
   * The copy constructor has to copy the state. If the other slot was occupied, a copy of the key
   * has to be made as well.
   */
  SimpleSetSlot(const SimpleSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new (this->key()) Key(*other.key());
    }
  }

  /**
   * The move constructor has to copy the state. If the other slot was occupied, the key from the
   * other slot has to be moved as well. The other slot stays in the state it was in before. Its
   * optionally stored key remains in a moved-from state.
   */
  SimpleSetSlot(SimpleSetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      new (this->key()) Key(std::move(*other.key()));
    }
  }

  /**
   * Get a non-const pointer to the position where the key is stored.
   */
  Key *key()
  {
    return (Key *)m_buffer.ptr();
  }

  /**
   * Get a const pointer to the position where the key is stored.
   */
  const Key *key() const
  {
    return (const Key *)m_buffer.ptr();
  }

  /**
   * Return true if the slot currently contains a key.
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
   * during the lifetime of a slot. In this simple set slot implementation, we just compute the
   * hash here. Other implementations might store the hash in the slot instead.
   */
  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(*this->key());
  }

  /**
   * Move the other slot into this slot and destruct it. We do destruction here, because this way
   * we can avoid a comparison with the state, since we know the slot is occupied.
   */
  void relocate_occupied_here(SimpleSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = Occupied;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  /**
   * Return true, when this slot is occupied and contains a key that compares equal to the given
   * key. The hash is used by other slot implementations to determine inequality faster.
   */
  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t UNUSED(hash)) const
  {
    if (m_state == Occupied) {
      return key == *this->key();
    }
    return false;
  }

  /**
   * Change the state of this slot from empty/removed to occupied. The key has to be constructed
   * by calling the constructor with the given key as parameter.
   */
  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    m_state = Occupied;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  /**
   * Change the state of this slot from occupied to removed. The key has to be destructed as well.
   */
  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = Removed;
    this->key()->~Key();
  }
};

/**
 * This set slot implementation stores the hash of the key within the slot. This helps when
 * computing the hash or an equality check is expensive.
 */
template<typename Key> class HashedSetSlot {
 private:
  enum State : uint8_t {
    Empty = 0,
    Occupied = 1,
    Removed = 2,
  };

  uint32_t m_hash;
  State m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  HashedSetSlot()
  {
    m_state = Empty;
  }

  ~HashedSetSlot()
  {
    if (m_state == Occupied) {
      this->key()->~Key();
    }
  }

  HashedSetSlot(const HashedSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      m_hash = other.m_hash;
      new (this->key()) Key(*other.key());
    }
  }

  HashedSetSlot(HashedSetSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == Occupied) {
      m_hash = other.m_hash;
      new (this->key()) Key(std::move(*other.key()));
    }
  }

  Key *key()
  {
    return (Key *)m_buffer.ptr();
  }

  const Key *key() const
  {
    return (const Key *)m_buffer.ptr();
  }

  bool is_occupied() const
  {
    return m_state == Occupied;
  }

  bool is_empty() const
  {
    return m_state == Empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &UNUSED(hash)) const
  {
    BLI_assert(this->is_occupied());
    return m_hash;
  }

  void relocate_occupied_here(HashedSetSlot &other, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = Occupied;
    m_hash = hash;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t hash) const
  {
    if (m_hash == hash) {
      if (m_state == Occupied) {
        return key == *this->key();
      }
    }
    return false;
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    m_state = Occupied;
    m_hash = hash;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = Removed;
    this->key()->~Key();
  }
};

/**
 * Pointers have special values that can be expected not to be used as keys. This set slot will use
 * those values to represent the empty and removed state. This saves memory and makes some
 * operations faster.
 */
template<typename Key> class PointerSetSlot {
 private:
  BLI_STATIC_ASSERT(std::is_pointer<Key>::value, "");

#define s_is_empty UINTPTR_MAX
#define s_is_removed (UINTPTR_MAX - 1)
#define s_min_special_value s_is_removed

  uintptr_t m_state;

 public:
  PointerSetSlot()
  {
    m_state = s_is_empty;
  }

  ~PointerSetSlot() = default;
  PointerSetSlot(const PointerSetSlot &other) = default;
  PointerSetSlot(PointerSetSlot &&other) = default;

  Key *key()
  {
    return (Key *)&m_state;
  }

  const Key *key() const
  {
    return (const Key *)&m_state;
  }

  bool is_occupied() const
  {
    return m_state < s_min_special_value;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash((Key)m_state);
  }

  void relocate_occupied_here(PointerSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = other.m_state;
  }

  bool contains(Key key, uint32_t UNUSED(hash)) const
  {
    BLI_assert((uintptr_t)key < s_min_special_value);
    return (uintptr_t)key == m_state;
  }

  void occupy(Key key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert((uintptr_t)key < s_min_special_value);
    m_state = (uintptr_t)key;
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = s_is_removed;
  }

#undef s_is_empty
#undef s_is_removed
#undef s_min_special_value
};

template<typename Key> struct DefaultSetSlot {
  using type = SimpleSetSlot<Key>;
};

template<> struct DefaultSetSlot<std::string> {
  using type = HashedSetSlot<std::string>;
};

template<typename Key> struct DefaultSetSlot<Key *> {
  using type = PointerSetSlot<Key *>;
};

}  // namespace BLI

#endif /* __BLI_SET_SLOTS_HH__ */
