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

#include <type_traits>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_vector.hh"

namespace BLI {

template<typename Value> struct DefaultSetSlot;

template<typename Value,
         uint32_t InlineBufferCapacity = 4,
         typename Hash = DefaultHash<Value>,
         typename Slot = typename DefaultSetSlot<Value>::type,
         typename Allocator = GuardedAllocator>
class Set {
 private:
  static constexpr uint32_t s_linear_probing_steps = 2;

  /* TODO: Round up to power of two. */
  static constexpr uint32_t s_default_slot_array_size = InlineBufferCapacity * 2;

  using SlotArray = Array<Slot, s_default_slot_array_size, Allocator>;
  SlotArray m_slots;

  uint32_t m_usable_slots;
  uint32_t m_set_or_dummy_slots;
  uint32_t m_dummy_slots;
  uint32_t m_slot_mask;

 public:
  Set()
  {
    m_slots = SlotArray(power_of_2_max_u(s_default_slot_array_size));

    m_set_or_dummy_slots = 0;
    m_dummy_slots = 0;
    m_usable_slots = m_slots.size() / 2;
    m_slot_mask = m_slots.size() - 1;
  }

  ~Set() = default;

  Set(const std::initializer_list<Value> &list) : Set()
  {
    this->add_multiple(list);
  }

  Set(const Set &other) = default;
  Set(Set &&other)
      : m_slots(std::move(other.m_slots)),
        m_usable_slots(other.m_usable_slots),
        m_set_or_dummy_slots(other.m_set_or_dummy_slots),
        m_dummy_slots(other.m_dummy_slots),
        m_slot_mask(other.m_slot_mask)
  {
    other.~Set();
    new (&other) Set();
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
    if (this->size() == 0) {
      std::cout << "  <empty>\n";
      return;
    }
    uint total_collisions = 0;
    for (uint32_t i : stats.index_range()) {
      std::cout << "  " << i << " Collisions: " << stats[i] << "\n";
      total_collisions += stats[i] * i;
    }
    std::cout << "  Average Collisions: " << (float)total_collisions / (float)this->size() << "\n";
  }

  void clear()
  {
    this->~Set();
    new (this) Set();
  }

  static bool Intersects(const Set &a, const Set &b)
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

  static bool Disjoint(const Set &a, const Set &b)
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
    uint32_t real_hash = old_slot.get_hash(Hash());
    uint32_t hash = real_hash;
    uint32_t perturb = real_hash;

    do {
      for (uint32_t i = 0; i < s_linear_probing_steps; i++) {
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
      for (uint32_t i = 0; i < s_linear_probing_steps; i++) {
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
      for (uint32_t i = 0; i < s_linear_probing_steps; i++) {
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
      for (uint32_t i = 0; i < s_linear_probing_steps; i++) {
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
      for (uint32_t i = 0; i < s_linear_probing_steps; i++) {
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
      for (uint32_t i = 0; i < s_linear_probing_steps; i++) {
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

template<typename Value> class SimpleSetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_set = 1;
  static constexpr uint8_t s_is_dummy = 2;

  uint8_t m_state;
  AlignedBuffer<sizeof(Value), alignof(Value)> m_buffer;

 public:
  SimpleSetSlot()
  {
    m_state = s_is_empty;
  }

  ~SimpleSetSlot()
  {
    if (m_state == s_is_set) {
      this->value()->~Value();
    }
  }

  SimpleSetSlot(const SimpleSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      new (this->value()) Value(*other.value());
    }
  }

  SimpleSetSlot(SimpleSetSlot &&other) noexcept
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      new (this->value()) Value(std::move(*other.value()));
    }
  }

  Value *value()
  {
    return (Value *)m_buffer.ptr();
  }

  const Value *value() const
  {
    return (const Value *)m_buffer.ptr();
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
    return hash(*this->value());
  }

  void set_and_destruct_other(SimpleSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_state = s_is_set;
    new (this->value()) Value(std::move(*other.value()));
    other.value()->~Value();
  }

  template<typename OtherValue> bool contains(const OtherValue &value, uint32_t UNUSED(hash)) const
  {
    if (m_state == s_is_set) {
      return value == *this->value();
    }
    return false;
  }

  template<typename ForwardValue> void set(ForwardValue &&value, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    m_state = s_is_set;
    new (this->value()) Value(std::forward<ForwardValue>(value));
  }

  void set_to_dummy()
  {
    BLI_assert(this->is_set());
    m_state = s_is_dummy;
    this->value()->~Value();
  }
};

template<typename Value> class HashedSetSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_set = 1;
  static constexpr uint8_t s_is_dummy = 2;

  uint32_t m_hash;
  uint8_t m_state;
  AlignedBuffer<sizeof(Value), alignof(Value)> m_buffer;

 public:
  HashedSetSlot()
  {
    m_state = s_is_empty;
  }

  ~HashedSetSlot()
  {
    if (m_state == s_is_set) {
      this->value()->~Value();
    }
  }

  HashedSetSlot(const HashedSetSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      m_hash = other.m_hash;
      new (this->value()) Value(*other.value());
    }
  }

  HashedSetSlot(HashedSetSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      m_hash = other.m_hash;
      new (this->value()) Value(std::move(*other.value()));
    }
  }

  Value *value()
  {
    return (Value *)m_buffer.ptr();
  }

  const Value *value() const
  {
    return (const Value *)m_buffer.ptr();
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
    new (this->value()) Value(std::move(*other.value()));
    other.value()->~Value();
  }

  template<typename OtherValue> bool contains(const OtherValue &value, uint32_t hash) const
  {
    if (m_hash == hash) {
      if (m_state == s_is_set) {
        return value == *this->value();
      }
    }
    return false;
  }

  template<typename ForwardValue> void set(ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->is_set());
    m_state = s_is_set;
    m_hash = hash;
    new (this->value()) Value(std::forward<ForwardValue>(value));
  }

  void set_to_dummy()
  {
    BLI_assert(this->is_set());
    m_state = s_is_dummy;
    this->value()->~Value();
  }
};

template<typename Value> class PointerSetSlot {
 private:
  BLI_STATIC_ASSERT(std::is_pointer<Value>::value, "");

  /* Note: nullptr is not a valid value. */
  static constexpr uintptr_t s_is_empty = 0;
  static constexpr uintptr_t s_is_dummy = 1;
  static constexpr uintptr_t s_max_special_value = s_is_dummy;

  uintptr_t m_value;

 public:
  PointerSetSlot()
  {
    m_value = s_is_empty;
  }

  ~PointerSetSlot() = default;
  PointerSetSlot(const PointerSetSlot &other) = default;
  PointerSetSlot(PointerSetSlot &&other) = default;

  Value *value()
  {
    return (Value *)&m_value;
  }

  const Value *value() const
  {
    return (const Value *)&m_value;
  }

  bool is_set() const
  {
    return m_value > s_max_special_value;
  }

  bool is_empty() const
  {
    return m_value == s_is_empty;
  }

  template<typename Hash> uint32_t get_hash(const Hash &hash) const
  {
    BLI_assert(this->is_set());
    return hash((Value)m_value);
  }

  void set_and_destruct_other(PointerSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_value = other.m_value;
  }

  bool contains(Value value, uint32_t UNUSED(hash)) const
  {
    BLI_assert((uintptr_t)value > s_max_special_value);
    return (uintptr_t)value == m_value;
  }

  void set(Value value, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert((uintptr_t)value > s_max_special_value);
    m_value = (uintptr_t)value;
  }

  void set_to_dummy()
  {
    BLI_assert(this->is_set());
    m_value = s_is_dummy;
  }
};

template<typename Value> struct DefaultSetSlot {
  using type = SimpleSetSlot<Value>;
};

template<> struct DefaultSetSlot<std::string> {
  using type = HashedSetSlot<std::string>;
};

template<typename Value> struct DefaultSetSlot<Value *> {
  using type = PointerSetSlot<Value *>;
};

}  // namespace BLI

#endif /* __BLI_SINGLE_SLOT_SET_HH__ */
