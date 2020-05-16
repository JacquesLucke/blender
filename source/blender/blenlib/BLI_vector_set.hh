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

#ifndef __BLI_VECTOR_SET_HH__
#define __BLI_VECTOR_SET_HH__

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_open_addressing.hh"

namespace BLI {

template<typename Key> class SimpleVectorSetSlot {
 private:
  static constexpr int32_t s_is_empty = -1;
  static constexpr int32_t s_is_dummy = -2;

  int32_t m_state = s_is_empty;

 public:
  bool is_set() const
  {
    return m_state >= 0;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  uint32_t index() const
  {
    BLI_assert(this->is_set());
    return m_state;
  }

  template<typename ForwardKey>
  bool contains(const ForwardKey &key, uint32_t UNUSED(hash), const Key *keys) const
  {
    if (m_state >= 0) {
      return key == keys[m_state];
    }
    return false;
  }

  void set_and_destruct_other(SimpleVectorSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_state = other.m_state;
  }

  void set(uint32_t index, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    m_state = (int32_t)index;
  }

  void update_index(uint32_t index)
  {
    BLI_assert(this->is_set());
    m_state = (int32_t)index;
  }

  void set_to_dummy()
  {
    m_state = s_is_dummy;
  }

  bool has_index(uint32_t index) const
  {
    return (uint32_t)m_state == index;
  }

  template<typename Hash> uint32_t get_hash(const Key &key, const Hash &hash) const
  {
    BLI_assert(this->is_set());
    return hash(key);
  }
};

template<typename Key,
         uint32_t InlineBufferCapacity = 4,
         typename Hash = DefaultHash<Key>,
         typename ProbingStrategy = DefaultProbingStrategy,
         typename Allocator = GuardedAllocator>
class VectorSet {
 private:
  static constexpr uint32_t s_max_load_factor_numerator = 1;
  static constexpr uint32_t s_max_load_factor_denominator = 2;
  static constexpr uint32_t s_default_slot_array_size = total_slot_amount_for_usable_slots(
      InlineBufferCapacity, s_max_load_factor_numerator, s_max_load_factor_denominator);

  using Slot = SimpleVectorSetSlot<Key>;
  using SlotArray = Array<Slot, s_default_slot_array_size, Allocator>;
  SlotArray m_slots;
  Key *m_keys;

  uint32_t m_dummy_slots;
  uint32_t m_set_or_dummy_slots;
  uint32_t m_usable_slots;
  uint32_t m_slot_mask;

 public:
  VectorSet()
  {
    BLI_assert(is_power_of_2_i((int)s_default_slot_array_size));
    m_slots = SlotArray(s_default_slot_array_size);

    m_dummy_slots = 0;
    m_set_or_dummy_slots = 0;
    m_usable_slots = floor_multiplication_with_fraction(
        m_slots.size(), s_max_load_factor_numerator, s_max_load_factor_denominator);
    m_slot_mask = m_slots.size() - 1;

    m_keys = this->allocate_keys_array(m_usable_slots);
  }

  VectorSet(const std::initializer_list<Key> &keys) : VectorSet()
  {
    this->add_multiple(keys);
  }

  ~VectorSet()
  {
    destruct_n(m_keys, this->size());
    this->deallocate_keys_array(m_keys);
  }

  VectorSet(const VectorSet &other)
      : m_slots(other.m_slots),
        m_dummy_slots(other.m_dummy_slots),
        m_set_or_dummy_slots(other.m_set_or_dummy_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask)
  {
    m_keys = this->allocate_keys_array(m_usable_slots);
    uninitialized_copy_n(other.m_keys, other.size(), m_keys);
  }

  VectorSet(VectorSet &&other)
      : m_slots(std::move(other.m_slots)),
        m_keys(other.m_keys),
        m_dummy_slots(other.m_dummy_slots),
        m_set_or_dummy_slots(other.m_set_or_dummy_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask)
  {
    other.m_slots = SlotArray(power_of_2_max_u(s_default_slot_array_size));

    other.m_dummy_slots = 0;
    other.m_set_or_dummy_slots = 0;
    other.m_usable_slots = other.m_slots.size() / 2;
    other.m_slot_mask = other.m_slots.size() - 1;

    other.m_keys = this->allocate_keys_array(other.m_usable_slots);
  }

  VectorSet &operator=(const VectorSet &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~VectorSet();
    new (this) VectorSet(other);

    return *this;
  }

  VectorSet &operator=(VectorSet &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~VectorSet();
    new (this) VectorSet(std::move(other));

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

  void add(const Key &key)
  {
    this->add__impl(key, Hash{}(key));
  }
  void add(Key &&key)
  {
    this->add__impl(std::move(key), Hash{}(key));
  }

  void add_multiple(ArrayRef<Key> keys)
  {
    for (const Key &key : keys) {
      this->add(key);
    }
  }

  bool contains(const Key &key) const
  {
    return this->contains__impl(key, Hash{}(key));
  }

  void remove(const Key &key)
  {
    this->remove__impl(key, Hash{}(key));
  }

  Key pop()
  {
    return this->pop__impl();
  }

  uint32_t index(const Key &key) const
  {
    return this->index__impl(key, Hash{}(key));
  }

  int32_t index_try(const Key &key) const
  {
    return this->index_try__impl(key, Hash{}(key));
  }

  const Key *begin() const
  {
    return m_keys;
  }

  const Key *end() const
  {
    return m_keys + this->size();
  }

  const Key &operator[](uint32_t index) const
  {
    BLI_assert(index <= this->size());
    return m_keys[index];
  }

  operator ArrayRef<Key>() const
  {
    return ArrayRef<Key>(m_keys, this->size());
  }

  ArrayRef<Key> as_ref() const
  {
    return *this;
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

    Key *new_keys = this->allocate_keys_array(usable_slots);
    relocate_n(m_keys, this->size(), new_keys);
    this->deallocate_keys_array(m_keys);

    m_slots.clear_without_destruct();
    m_slots = std::move(new_slots);
    m_keys = new_keys;
    m_set_or_dummy_slots -= m_dummy_slots;
    m_usable_slots = usable_slots;
    m_dummy_slots = 0;
    m_slot_mask = new_slot_mask;
  }

  void add_after_grow_and_destruct_old(Slot &old_slot,
                                       SlotArray &new_slots,
                                       uint32_t new_slot_mask)
  {
    const Key &key = m_keys[old_slot.index()];
    uint32_t hash = old_slot.get_hash(key, Hash());

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
      if (slot.contains(key, hash, m_keys)) {
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
        uint32_t index = this->size();
        new (m_keys + index) Key(std::forward<ForwardKey>(key));
        slot.set(index, hash);
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
        uint32_t index = this->size();
        new (m_keys + index) Key(std::forward<ForwardKey>(key));
        m_set_or_dummy_slots++;
        slot.set(index, hash);
        return true;
      }
      if (slot.contains(key, hash, m_keys)) {
        return false;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> uint32_t index__impl(const ForwardKey &key, uint32_t hash) const
  {
    BLI_assert(this->contains(key));

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      const Slot &slot = m_slots[slot_index];
      if (slot.contains(key, hash, m_keys)) {
        return slot.index();
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> int32_t index_try__impl(const ForwardKey &key, uint32_t hash) const
  {
    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      const Slot &slot = m_slots[slot_index];
      if (slot.contains(key, hash, m_keys)) {
        return slot.index();
      }
      if (slot.is_empty()) {
        return -1;
      }
    }
    SLOT_PROBING_END();
  }

  Key pop__impl()
  {
    BLI_assert(this->size() > 0);

    uint32_t index_to_pop = this->size() - 1;
    Key key = std::move(m_keys[index_to_pop]);
    destruct(m_keys + index_to_pop);
    uint32_t hash = Hash{}(key);

    m_dummy_slots++;

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.has_index(index_to_pop)) {
        slot.set_to_dummy();
        return key;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains(key));

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.contains(key, hash, m_keys)) {
        uint32_t index_to_remove = slot.index();
        uint32_t size = this->size();
        uint32_t last_element_index = size - 1;

        if (index_to_remove < last_element_index) {
          m_keys[index_to_remove] = std::move(m_keys[last_element_index]);
          this->update_slot_index(m_keys[index_to_remove], last_element_index, index_to_remove);
        }

        destruct(m_keys + last_element_index);
        slot.set_to_dummy();
        m_dummy_slots++;
        return;
      }
    }
    SLOT_PROBING_END();
  }

  void update_slot_index(const Key &key, uint32_t old_index, uint32_t new_index)
  {
    uint32_t hash = Hash{}(key);
    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.has_index(old_index)) {
        slot.update_index(new_index);
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

  Key *allocate_keys_array(uint32_t size)
  {
    return (Key *)m_slots.allocator().allocate_aligned(
        (uint32_t)sizeof(Key) * size, alignof(Key), __func__);
  }

  void deallocate_keys_array(Key *keys)
  {
    m_slots.allocator().deallocate(keys);
  }
};

}  // namespace BLI

#endif /* __BLI_VECTOR_SET_HH__ */
