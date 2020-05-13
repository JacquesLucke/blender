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

#ifndef __BLI_SINGLE_SLOT_SET_HH__
#define __BLI_SINGLE_SLOT_SET_HH__

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_vector.hh"

namespace BLI {

template<typename Value, typename Hash> class DefaultMySetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_set = 1;
  static constexpr uint8_t s_is_dummy = 2;

  uint8_t m_state;
  AlignedBuffer<sizeof(Value), alignof(Value)> m_buffer;

 public:
  DefaultMySetSlot()
  {
    m_state = s_is_empty;
  }

  ~DefaultMySetSlot()
  {
    if (m_state == s_is_set) {
      this->value()->~Value();
    }
  }

  DefaultMySetSlot(const DefaultMySetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      new (this->value()) Value(*other.value());
    }
  }

  DefaultMySetSlot(DefaultMySetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      new (this->value()) Value(std::move(*other.value()));
    }
  }

  Value *value() const
  {
    return (Value *)m_buffer.ptr();
  }

  bool is_set() const
  {
    return m_state == s_is_set;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  uint32_t get_hash() const
  {
    BLI_assert(m_state == s_is_set);
    return Hash{}(*this->value());
  }

  void set_and_destruct_other(DefaultMySetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(m_state != s_is_set);
    BLI_assert(other.m_state == s_is_set);
    m_state = s_is_set;
    new (this->value()) Value(std::move(*other.value()));
    other.value()->~Value();
  }

  template<typename OtherValue> bool contains(const OtherValue &value, uint32_t UNUSED(hash)) const
  {
    if (m_state == s_is_set) {
      return value == *this->value();
    }
    else {
      return false;
    }
  }

  template<typename ForwardValue> void set(ForwardValue &&value, uint32_t UNUSED(hash))
  {
    BLI_assert(m_state != s_is_set);
    m_state = s_is_set;
    new (this->value()) Value(std::forward<ForwardValue>(value));
  }

  void set_to_dummy()
  {
    BLI_assert(m_state == s_is_set);
    m_state = s_is_dummy;
    this->value()->~Value();
  }
};

template<typename Value,
         typename Hash = DefaultHash<Value>,
         //  typename Slot = DefaultMySetSlot<Value, Hash>,
         typename Allocator = GuardedAllocator>
class MySet {
 private:
  using Slot = DefaultMySetSlot<Value, Hash>;
  using SlotArray = Array<Slot, 16, Allocator>;
  SlotArray m_slots;

  static constexpr uint32_t s_linear_probing = 2;

  uint32_t m_usable_slots;
  uint32_t m_set_or_dummy_slots;
  uint32_t m_dummy_slots;
  uint32_t m_slot_mask;

 public:
  MySet()
  {
    m_slots = SlotArray(16);

    m_set_or_dummy_slots = 0;
    m_dummy_slots = 0;
    m_usable_slots = m_slots.size() / 2;
    m_slot_mask = m_slots.size() - 1;
  }

  ~MySet()
  {
    // this->print_collision_stats();
  }

  MySet(const std::initializer_list<Value> &list) : MySet()
  {
    this->add_multiple(list);
  }

  MySet(const MySet &other) = default;
  MySet(MySet &&other)
      : m_slots(std::move(other.m_slots)),
        m_usable_slots(other.m_usable_slots),
        m_set_or_dummy_slots(other.m_set_or_dummy_slots),
        m_dummy_slots(other.m_dummy_slots),
        m_slot_mask(other.m_slot_mask)
  {
    other.~MySet();
    new (&other) MySet();
  }

  uint32_t size() const
  {
    return m_set_or_dummy_slots - m_dummy_slots;
  }

  bool is_empty() const
  {
    return m_set_or_dummy_slots == m_dummy_slots;
  }

  void add_new(const Value &value)
  {
    this->add_new__impl(value, Hash{}(value));
  }
  void add_new(Value &&value)
  {
    this->add_new__impl(std::move(value), Hash{}(value));
  }

  bool add(const Value &value)
  {
    return this->add__impl(value, Hash{}(value));
  }
  bool add(Value &&value)
  {
    return this->add__impl(std::move(value), Hash{}(value));
  }

  void add_multiple(ArrayRef<Value> values)
  {
    for (const Value &value : values) {
      this->add(value);
    }
  }

  void add_multiple_new(ArrayRef<Value> values)
  {
    for (const Value &value : values) {
      this->add_new(value);
    }
  }

  bool contains(const Value &value) const
  {
    return this->contains__impl(value, Hash{}(value));
  }

  void remove(const Value &value)
  {
    return this->remove__impl(value, Hash{}(value));
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
      m_current_slot = this->next_set_slot_index(m_current_slot);
      return *this;
    }

    const Value &operator*() const
    {
      return *m_slots[m_current_slot].value();
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.m_slots == b.m_slots);
      BLI_assert(a.m_total_slots == b.m_total_slots);
      return a.m_current_slot != b.m_current_slot;
    }

   private:
    uint32_t next_set_slot_index(uint32_t slot_index) const
    {
      while (++slot_index < m_total_slots) {
        if (m_slots[slot_index].is_set()) {
          return slot_index;
        }
      }
      return slot_index;
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

  void print_collision_stats() const
  {
    Vector<uint32_t> stats = this->get_collision_stats();
    std::cout << "Collisions stats:\n";
    uint total_collisions = 0;
    for (uint32_t i : stats.index_range()) {
      std::cout << "  " << i << " Collisions: " << stats[i] << "\n";
      total_collisions += stats[i];
    }
    std::cout << "  Average Collisions: " << (float)total_collisions / (float)this->size() << "\n";
  }

  void clear()
  {
    this->~MySet();
    new (this) MySet();
  }

  static bool Intersects(const MySet &a, const MySet &b)
  {
    /* Make sure we iterate over the shorter set. */
    if (a.size() > b.size()) {
      return Intersects(b, a);
    }

    for (const Value &value : a) {
      if (b.contains(value)) {
        return true;
      }
    }
    return false;
  }

  static bool Disjoint(const MySet &a, const MySet &b)
  {
    return !Intersects(a, b);
  }

 private:
  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    min_usable_slots = power_of_2_max_u(min_usable_slots);
    uint32_t total_slots = min_usable_slots * 2;

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
    m_usable_slots = min_usable_slots;
    m_dummy_slots = 0;
    m_slot_mask = new_slot_mask;
  }

  void add_after_grow_and_destruct_old(Slot &old_slot,
                                       SlotArray &new_slots,
                                       uint32_t new_slot_mask)
  {
    uint32_t real_hash = old_slot.get_hash();
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    do {
      for (uint32_t i = 0; i < s_linear_probing; i++) {
        uint32_t slot_index = (hash + i) & new_slot_mask;
        Slot &slot = new_slots[slot_index];
        if (slot.is_empty()) {
          slot.set_and_destruct_other(old_slot, real_hash);
          return;
        }
      }

      perturb >>= 5;
      hash = hash * 5 + 1 + perturb;
    } while (true);
  }

  template<typename OtherValue>
  bool contains__impl(const OtherValue &value, uint32_t real_hash) const
  {
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    do {

      for (uint32_t i = 0; i < s_linear_probing; i++) {
        uint32_t slot_index = (hash + i) & m_slot_mask;
        const Slot &slot = m_slots[slot_index];
        if (slot.is_empty()) {
          return false;
        }
        if (slot.contains(value, real_hash)) {
          return true;
        }
      }

      perturb >>= 5;
      hash = hash * 5 + 1 + perturb;
    } while (true);
  }

  template<typename ForwardValue> void add_new__impl(ForwardValue &&value, uint32_t real_hash)
  {
    BLI_assert(!this->contains(value));
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    this->ensure_can_add();
    m_set_or_dummy_slots++;

    do {
      for (uint32_t i = 0; i < s_linear_probing; i++) {
        uint32_t slot_index = (hash + i) & m_slot_mask;
        Slot &slot = m_slots[slot_index];
        if (slot.is_empty()) {
          slot.set(std::forward<ForwardValue>(value), real_hash);
          return;
        }
      }

      perturb >>= 5;
      hash = hash * 5 + 1 + perturb;
    } while (true);
  }

  template<typename ForwardValue> bool add__impl(ForwardValue &&value, uint32_t real_hash)
  {
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    this->ensure_can_add();

    do {
      for (uint32_t i = 0; i < s_linear_probing; i++) {
        uint32_t slot_index = (hash + i) & m_slot_mask;
        Slot &slot = m_slots[slot_index];
        if (slot.is_empty()) {
          slot.set(std::forward<ForwardValue>(value), real_hash);
          m_set_or_dummy_slots++;
          return true;
        }
        if (slot.contains(std::forward<ForwardValue>(value), real_hash)) {
          return false;
        }
      }

      perturb >>= 5;
      hash = hash * 5 + 1 + perturb;
    } while (true);
  }

  template<typename OtherValue> void remove__impl(const OtherValue &value, uint32_t real_hash)
  {
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    m_dummy_slots++;

    do {
      for (uint32_t i = 0; i < s_linear_probing; i++) {
        uint32_t slot_index = (hash + i) & m_slot_mask;
        Slot &slot = m_slots[slot_index];
        if (slot.contains(value, real_hash)) {
          slot.set_to_dummy();
          return;
        }
      }

      perturb >>= 5;
      hash = hash * 5 + 1 + perturb;
    } while (true);
  }

  uint32_t count_collisions(const Value &value) const
  {
    uint32_t real_hash = Hash{}(value);
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    uint32_t collisions = 0;

    do {
      for (uint32_t i = 0; i < s_linear_probing; i++) {
        uint32_t slot_index = (hash + i) & m_slot_mask;
        const Slot &slot = m_slots[slot_index];
        if (slot.contains(value, real_hash)) {
          return collisions;
        }
        if (slot.is_empty()) {
          return collisions;
        }
        collisions++;
      }

      perturb >>= 5;
      hash = hash * 5 + 1 + perturb;
    } while (true);
  }

  Vector<uint32_t> get_collision_stats() const
  {
    Vector<uint32_t> stats;
    for (const Value &value : *this) {
      uint32_t collisions = this->count_collisions(value);
      if (stats.size() <= collisions) {
        stats.append_n_times(0, collisions - stats.size() + 1);
      }
      stats[collisions]++;
    }
    return stats;
  }

  void ensure_can_add()
  {
    if (m_set_or_dummy_slots >= m_usable_slots) {
      this->grow(this->size() + 1);
    }
  }
};

}  // namespace BLI

#endif /* __BLI_SINGLE_SLOT_SET_HH__ */
