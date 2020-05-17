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

#ifndef __BLI_SET_HH__
#define __BLI_SET_HH__

/** \file
 * \ingroup bli
 *
 * A `BLI::Set<Key>` is an unordered container for elements of type `Key`. It is designed to be a
 * more convenient and efficient replacement for `std::unordered_set`. All core operations (add,
 * remove and contains) can be done in O(1) expected time.
 *
 * In most cases, your default choice for a hash set in Blender should be `BLI::Set`.
 *
 * The implementation uses open addressing in a flat array. Probing is done using a combination of
 * linear probing and rehashing. The number of slots is always a power of two. `&` is used to
 * compute the slot index based the initial hash. Therefore, the hash function is expected to
 * provide a good distribution in the lower bits. If it does not do so, rehashing will kick in
 * after x linear probing steps. Then higher bits will be taken into account as well.
 */

#include <type_traits>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_open_addressing.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

namespace BLI {

template<typename Key> struct DefaultSetSlot;

template<typename Key,
         uint32_t InlineBufferCapacity = 4,
         typename Hash = DefaultHash<Key>,
         typename Slot = typename DefaultSetSlot<Key>::type,
         typename ProbingStrategy = DefaultProbingStrategy,
         typename Allocator = GuardedAllocator>
class Set {
 private:
  static constexpr uint32_t s_max_load_factor_numerator = 1;
  static constexpr uint32_t s_max_load_factor_denominator = 2;
  static constexpr uint32_t s_default_slot_array_size = total_slot_amount_for_usable_slots(
      InlineBufferCapacity, s_max_load_factor_numerator, s_max_load_factor_denominator);

  using SlotArray = Array<Slot, s_default_slot_array_size, Allocator>;
  SlotArray m_slots;

  uint32_t m_dummy_slots;
  uint32_t m_set_or_dummy_slots;
  uint32_t m_usable_slots;
  uint32_t m_slot_mask;

 public:
  Set()
  {
    BLI_assert(is_power_of_2_i((int)s_default_slot_array_size));
    m_slots = SlotArray(s_default_slot_array_size);

    m_dummy_slots = 0;
    m_set_or_dummy_slots = 0;
    m_usable_slots = floor_multiplication_with_fraction(
        m_slots.size(), s_max_load_factor_numerator, s_max_load_factor_denominator);
    m_slot_mask = m_slots.size() - 1;
  }

  ~Set() = default;

  Set(const std::initializer_list<Key> &list) : Set()
  {
    this->add_multiple(list);
  }

  Set(const Set &other) = default;

  Set(Set &&other)
      : m_slots(std::move(other.m_slots)),
        m_dummy_slots(other.m_dummy_slots),
        m_set_or_dummy_slots(other.m_set_or_dummy_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask)
  {
    other.~Set();
    new (&other) Set();
  }

  Set &operator=(const Set &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Set();
    new (this) Set(other);

    return *this;
  }

  Set &operator=(Set &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Set();
    new (this) Set(std::move(other));

    return *this;
  }

  uint32_t size() const
  {
    return m_set_or_dummy_slots - m_dummy_slots;
  }

  bool is_empty() const
  {
    return m_set_or_dummy_slots == m_dummy_slots;
  }

  uint32_t capacity() const
  {
    return m_slots.size();
  }

  uint32_t dummy_amount() const
  {
    return m_dummy_slots;
  }

  uint32_t size_per_element() const
  {
    return sizeof(Slot);
  }

  uint32_t size_in_bytes() const
  {
    return sizeof(Slot) * m_slots.size();
  }

  void reserve(uint32_t min_usable_slots)
  {
    if (m_usable_slots < min_usable_slots) {
      this->grow(min_usable_slots);
    }
  }

  void add_new(const Key &key)
  {
    this->add_new__impl(key, Hash{}(key));
  }
  void add_new(Key &&key)
  {
    this->add_new__impl(std::move(key), Hash{}(key));
  }

  bool add(const Key &key)
  {
    return this->add__impl(key, Hash{}(key));
  }
  bool add(Key &&key)
  {
    return this->add__impl(std::move(key), Hash{}(key));
  }

  void add_multiple(ArrayRef<Key> keys)
  {
    for (const Key &key : keys) {
      this->add(key);
    }
  }

  void add_multiple_new(ArrayRef<Key> keys)
  {
    for (const Key &key : keys) {
      this->add_new(key);
    }
  }

  bool contains(const Key &key) const
  {
    return this->contains__impl(key, Hash{}(key));
  }

  void remove(const Key &key)
  {
    return this->remove__impl(key, Hash{}(key));
  }

  class Iterator {
   private:
    const Slot *m_slots;
    uint32_t m_total_slots;
    uint32_t m_current_slot;

   public:
    Iterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : m_slots(slots), m_total_slots(total_slots), m_current_slot(current_slot)
    {
    }

    Iterator &operator++()
    {
      while (++m_current_slot < m_total_slots) {
        if (m_slots[m_current_slot].is_set()) {
          break;
        }
      }
      return *this;
    }

    const Key &operator*() const
    {
      return *m_slots[m_current_slot].key();
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.m_slots == b.m_slots);
      BLI_assert(a.m_total_slots == b.m_total_slots);
      return a.m_current_slot != b.m_current_slot;
    }
  };

  Iterator begin() const
  {
    for (uint32_t i = 0; i < m_slots.size(); i++) {
      if (m_slots[i].is_set()) {
        return Iterator(m_slots.begin(), m_slots.size(), i);
      }
    }
    return this->end();
  }

  Iterator end() const
  {
    return Iterator(m_slots.begin(), m_slots.size(), m_slots.size());
  }

  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, *this);
    stats.print();
  }

  uint32_t count_collisions(const Key &key) const
  {
    uint32_t hash = Hash{}(key);
    uint32_t collisions = 0;

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      const Slot &slot = m_slots[slot_index];
      if (slot.contains(key, hash)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    SLOT_PROBING_END();
  }

  void clear()
  {
    this->~Set();
    new (this) Set();
  }

  void rehash()
  {
    this->grow(this->size());
  }

  static bool Intersects(const Set &a, const Set &b)
  {
    /* Make sure we iterate over the shorter set. */
    if (a.size() > b.size()) {
      return Intersects(b, a);
    }

    for (const Key &key : a) {
      if (b.contains(key)) {
        return true;
      }
    }
    return false;
  }

  static bool Disjoint(const Set &a, const Set &b)
  {
    return !Intersects(a, b);
  }

 private:
  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    uint32_t total_slots = total_slot_amount_for_usable_slots(
        min_usable_slots, s_max_load_factor_numerator, s_max_load_factor_denominator);
    uint32_t usable_slots = floor_multiplication_with_fraction(
        total_slots, s_max_load_factor_numerator, s_max_load_factor_denominator);

    SlotArray new_slots(total_slots);
    uint32_t new_slot_mask = total_slots - 1;

    for (Slot &slot : m_slots) {
      if (slot.is_set()) {
        this->add_after_grow_and_destruct_old(slot, new_slots, new_slot_mask);
      }
    }

    m_slots.clear_without_destruct();
    m_slots = std::move(new_slots);
    m_set_or_dummy_slots -= m_dummy_slots;
    m_usable_slots = usable_slots;
    m_dummy_slots = 0;
    m_slot_mask = new_slot_mask;
  }

  void add_after_grow_and_destruct_old(Slot &old_slot,
                                       SlotArray &new_slots,
                                       uint32_t new_slot_mask)
  {
    uint32_t hash = old_slot.get_hash(Hash());

    SLOT_PROBING_BEGIN (hash, new_slot_mask, slot_index) {
      Slot &slot = new_slots[slot_index];
      if (slot.is_empty()) {
        slot.set_and_destruct_other(old_slot, hash);
        return;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool contains__impl(const ForwardKey &key, uint32_t hash) const
  {
    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      const Slot &slot = m_slots[slot_index];
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, hash)) {
        return true;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> void add_new__impl(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->contains(key));

    this->ensure_can_add();

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.is_empty()) {
        slot.set(std::forward<ForwardKey>(key), hash);
        m_set_or_dummy_slots++;
        return;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool add__impl(ForwardKey &&key, uint32_t hash)
  {
    this->ensure_can_add();

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.is_empty()) {
        slot.set(std::forward<ForwardKey>(key), hash);
        m_set_or_dummy_slots++;
        return true;
      }
      if (slot.contains(key, hash)) {
        return false;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains(key));
    m_dummy_slots++;

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.contains(key, hash)) {
        slot.set_to_dummy();
        return;
      }
    }
    SLOT_PROBING_END();
  }

  void ensure_can_add()
  {
    if (m_set_or_dummy_slots >= m_usable_slots) {
      this->grow(this->size() + 1);
    }
  }
};

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

#endif /* __BLI_SET_HH__ */
