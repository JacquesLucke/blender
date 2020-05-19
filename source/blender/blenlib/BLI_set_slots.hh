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
 */

#include "BLI_memory_utils.hh"

namespace BLI {

template<typename Key> class SimpleSetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_occupied = 1;
  static constexpr uint8_t s_is_removed = 2;

  uint8_t m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  SimpleSetSlot()
  {
    m_state = s_is_empty;
  }

  ~SimpleSetSlot()
  {
    if (m_state == s_is_occupied) {
      this->key()->~Key();
    }
  }

  SimpleSetSlot(const SimpleSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_occupied) {
      new (this->key()) Key(*other.key());
    }
  }

  SimpleSetSlot(SimpleSetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == s_is_occupied) {
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
    return m_state == s_is_occupied;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash(*this->key());
  }

  void relocate_occupied_here(SimpleSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_state = s_is_occupied;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t UNUSED(hash)) const
  {
    if (m_state == s_is_occupied) {
      return key == *this->key();
    }
    return false;
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t UNUSED(hash))
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
  }
};

template<typename Key> class HashedSetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_occupied = 1;
  static constexpr uint8_t s_is_removed = 2;

  uint32_t m_hash;
  uint8_t m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  HashedSetSlot()
  {
    m_state = s_is_empty;
  }

  ~HashedSetSlot()
  {
    if (m_state == s_is_occupied) {
      this->key()->~Key();
    }
  }

  HashedSetSlot(const HashedSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_occupied) {
      m_hash = other.m_hash;
      new (this->key()) Key(*other.key());
    }
  }

  HashedSetSlot(HashedSetSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_occupied) {
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
    return m_state == s_is_occupied;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
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
    m_state = s_is_occupied;
    m_hash = hash;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t hash) const
  {
    if (m_hash == hash) {
      if (m_state == s_is_occupied) {
        return key == *this->key();
      }
    }
    return false;
  }

  template<typename ForwardKey> void occupy(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->is_occupied());
    m_state = s_is_occupied;
    m_hash = hash;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    m_state = s_is_removed;
    this->key()->~Key();
  }
};

template<typename Key> class PointerSetSlot {
 private:
  BLI_STATIC_ASSERT(std::is_pointer<Key>::value, "");

  /* Note: nullptr is not a valid key. */
  static constexpr uintptr_t s_is_empty = 0;
  static constexpr uintptr_t s_is_removed = 1;
  static constexpr uintptr_t s_max_special_value = s_is_removed;

  uintptr_t m_key;

 public:
  PointerSetSlot()
  {
    m_key = s_is_empty;
  }

  ~PointerSetSlot() = default;
  PointerSetSlot(const PointerSetSlot &other) = default;
  PointerSetSlot(PointerSetSlot &&other) = default;

  Key *key()
  {
    return (Key *)&m_key;
  }

  const Key *key() const
  {
    return (const Key *)&m_key;
  }

  bool is_occupied() const
  {
    return m_key > s_max_special_value;
  }

  bool is_empty() const
  {
    return m_key == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_occupied());
    return hash((Key)m_key);
  }

  void relocate_occupied_here(PointerSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert(other.is_occupied());
    m_key = other.m_key;
  }

  bool contains(Key key, uint32_t UNUSED(hash)) const
  {
    BLI_assert((uintptr_t)key > s_max_special_value);
    return (uintptr_t)key == m_key;
  }

  void occupy(Key key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_occupied());
    BLI_assert((uintptr_t)key > s_max_special_value);
    m_key = (uintptr_t)key;
  }

  void remove()
  {
    BLI_assert(this->is_occupied());
    m_key = s_is_removed;
  }
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
