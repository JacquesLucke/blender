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
 */

#include "BLI_memory_utils.hh"

namespace BLI {

template<typename Key> class SimpleSetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_set = 1;
  static constexpr uint8_t s_is_dummy = 2;

  uint8_t m_state;
  AlignedBuffer<sizeof(Key), alignof(Key)> m_buffer;

 public:
  SimpleSetSlot()
  {
    m_state = s_is_empty;
  }

  ~SimpleSetSlot()
  {
    if (m_state == s_is_set) {
      this->key()->~Key();
    }
  }

  SimpleSetSlot(const SimpleSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      new (this->key()) Key(*other.key());
    }
  }

  SimpleSetSlot(SimpleSetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
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

  bool is_set() const
  {
    return m_state == s_is_set;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_set());
    return hash(*this->key());
  }

  void set_and_destruct_other(SimpleSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_state = s_is_set;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t UNUSED(hash)) const
  {
    if (m_state == s_is_set) {
      return key == *this->key();
    }
    return false;
  }

  template<typename ForwardKey> void set(ForwardKey &&key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    m_state = s_is_set;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  void set_to_dummy()
  {
    BLI_assert(this->is_set());
    m_state = s_is_dummy;
    this->key()->~Key();
  }
};

template<typename Key> class HashedSetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_set = 1;
  static constexpr uint8_t s_is_dummy = 2;

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
    if (m_state == s_is_set) {
      this->key()->~Key();
    }
  }

  HashedSetSlot(const HashedSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      m_hash = other.m_hash;
      new (this->key()) Key(*other.key());
    }
  }

  HashedSetSlot(HashedSetSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
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

  bool is_set() const
  {
    return m_state == s_is_set;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &UNUSED(hash)) const
  {
    BLI_assert(this->is_set());
    return m_hash;
  }

  void set_and_destruct_other(HashedSetSlot &other, uint32_t hash)
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_state = s_is_set;
    m_hash = hash;
    new (this->key()) Key(std::move(*other.key()));
    other.key()->~Key();
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key, uint32_t hash) const
  {
    if (m_hash == hash) {
      if (m_state == s_is_set) {
        return key == *this->key();
      }
    }
    return false;
  }

  template<typename ForwardKey> void set(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->is_set());
    m_state = s_is_set;
    m_hash = hash;
    new (this->key()) Key(std::forward<ForwardKey>(key));
  }

  void set_to_dummy()
  {
    BLI_assert(this->is_set());
    m_state = s_is_dummy;
    this->key()->~Key();
  }
};

template<typename Key> class PointerSetSlot {
 private:
  BLI_STATIC_ASSERT(std::is_pointer<Key>::value, "");

  /* Note: nullptr is not a valid key. */
  static constexpr uintptr_t s_is_empty = 0;
  static constexpr uintptr_t s_is_dummy = 1;
  static constexpr uintptr_t s_max_special_value = s_is_dummy;

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

  bool is_set() const
  {
    return m_key > s_max_special_value;
  }

  bool is_empty() const
  {
    return m_key == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_set());
    return hash((Key)m_key);
  }

  void set_and_destruct_other(PointerSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_key = other.m_key;
  }

  bool contains(Key key, uint32_t UNUSED(hash)) const
  {
    BLI_assert((uintptr_t)key > s_max_special_value);
    return (uintptr_t)key == m_key;
  }

  void set(Key key, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert((uintptr_t)key > s_max_special_value);
    m_key = (uintptr_t)key;
  }

  void set_to_dummy()
  {
    BLI_assert(this->is_set());
    m_key = s_is_dummy;
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
