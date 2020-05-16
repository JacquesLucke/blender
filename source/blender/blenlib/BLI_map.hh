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

#ifndef __BLI_SINGLE_SLOT_MAP_HH__
#define __BLI_SINGLE_SLOT_MAP_HH__

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_open_addressing.hh"

namespace BLI {

template<typename Key, typename Value> struct DefaultMapSlot;

template<typename Key,
         typename Value,
         typename Hash = DefaultHash<Key>,
         typename Slot = typename DefaultMapSlot<Key, Value>::type,
         typename ProbingStrategy = DefaultProbingStrategy,
         typename Allocator = GuardedAllocator>
class Map {
 private:
  static constexpr uint32_t s_default_slot_array_size = 4;

  using SlotArray = Array<Slot, s_default_slot_array_size, Allocator>;
  SlotArray m_slots;

  uint32_t m_dummy_slots;
  uint32_t m_set_or_dummy_slots;
  uint32_t m_usable_slots;
  uint32_t m_slot_mask;

 public:
  Map()
  {
    m_slots = SlotArray(power_of_2_max_u(s_default_slot_array_size));

    m_dummy_slots = 0;
    m_set_or_dummy_slots = 0;
    m_usable_slots = m_slots.size() / 2;
    m_slot_mask = m_slots.size() - 1;
  }

  ~Map() = default;

  Map(const Map &other) = default;

  Map(Map &&other)
      : m_slots(std::move(other.m_slots)),
        m_dummy_slots(other.m_dummy_slots),
        m_set_or_dummy_slots(other.m_set_or_dummy_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask)
  {
    other.~Map();
    new (&other) Map();
  }

  Map &operator=(const Map &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Map();
    new (this) Map(other);

    return *this;
  }

  Map &operator=(Map &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~Map();
    new (this) Map(std::move(other));

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

  bool contains(const Key &key) const
  {
    return this->contains__impl(key, Hash{}(key));
  }

  void clear()
  {
    this->~Map();
    new (this) Map();
  }

  void add_new(const Key &key, const Value &value)
  {
    this->add_new__impl(key, value, Hash{}(key));
  }
  void add_new(const Key &key, Value &&value)
  {
    this->add_new__impl(key, std::move(value), Hash{}(key));
  }
  void add_new(Key &&key, const Value &value)
  {
    this->add_new__impl(std::move(key), value, Hash{}(key));
  }
  void add_new(Key &&key, Value &&value)
  {
    this->add_new__impl(std::move(key), std::move(value), Hash{}(key));
  }

  bool add(const Key &key, const Value &value)
  {
    return this->add__impl(key, value, Hash{}(key));
  }
  bool add(const Key &key, Value &&value)
  {
    return this->add__impl(key, std::move(value), Hash{}(key));
  }
  bool add(Key &&key, const Value &value)
  {
    return this->add__impl(std::move(key), value, Hash{}(key));
  }
  bool add(Key &&key, Value &&value)
  {
    return this->add__impl(std::move(key), std::move(value), Hash{}(key));
  }

  void remove(const Key &key)
  {
    this->remove__impl(key, Hash{}(key));
  }

  Value pop(const Key &key)
  {
    return this->pop__impl(key, Hash{}(key));
  }

  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(const Key &key,
                     const CreateValueF &create_value,
                     const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify__impl(key, create_value, modify_value, Hash{}(key));
  }
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(Key &&key, const CreateValueF &create_value, const ModifyValueF &modify_value)
      -> decltype(create_value(nullptr))
  {
    return this->add_or_modify__impl(std::move(key), create_value, modify_value, Hash{}(key));
  }

  bool add_override(const Key &key, const Value &value)
  {
    return this->add_override__impl(key, value, Hash{}(key));
  }
  bool add_override(const Key &key, Value &&value)
  {
    return this->add_override__impl(key, std::move(value), Hash{}(key));
  }
  bool add_override(Key &&key, const Value &value)
  {
    return this->add_override__impl(std::move(key), value, Hash{}(key));
  }
  bool add_override(Key &&key, Value &&value)
  {
    return this->add_override__impl(std::move(key), std::move(value), Hash{}(key));
  }

  const Value *lookup_ptr(const Key &key) const
  {
    return this->lookup_ptr__impl(key, Hash{}(key));
  }

  Value *lookup_ptr(const Key &key)
  {
    const Map *const_this = this;
    return const_cast<Value *>(const_this->lookup_ptr(key));
  }

  const Value &lookup(const Key &key) const
  {
    const Value *ptr = this->lookup_ptr(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }

  Value &lookup(const Key &key)
  {
    Value *ptr = this->lookup_ptr(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }

  Value lookup_default(const Key &key, Value default_value) const
  {
    const Value *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return default_value;
    }
  }

  template<typename CreateValueF>
  Value &lookup_or_add(const Key &key, const CreateValueF &create_value)
  {
    return this->lookup_or_add__impl(key, create_value, Hash{}(key));
  }
  template<typename CreateValueF> Value &lookup_or_add(Key &&key, const CreateValueF &create_value)
  {
    return this->lookup_or_add__impl(std::move(key), create_value, Hash{}(key));
  }

  Value &lookup_or_add_default(const Key &key)
  {
    return this->lookup_or_add(key, []() { return Value(); });
  }
  Value &lookup_or_add_default(const Key &&key)
  {
    return this->lookup_or_add(std::move(key), []() { return Value(); });
  }

  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    uint32_t size = this->size();
    for (uint32_t i = 0; i < size; i++) {
      const Slot &slot = m_slots[i];
      if (slot.is_set()) {
        const Key &key = *slot.key();
        const Value &value = *slot.value();
        func(key, value);
      }
    }
  }

  template<typename SubIterator> struct BaseIterator {
    Slot *m_slots;
    uint32_t m_total_slots;
    uint32_t m_current_slot;

    BaseIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : m_slots(const_cast<Slot *>(slots)),
          m_total_slots(total_slots),
          m_current_slot(current_slot)
    {
    }

    BaseIterator &operator++()
    {
      while (++m_current_slot < m_total_slots) {
        if (m_slots[m_current_slot].is_set()) {
          break;
        }
      }
      return *this;
    }

    friend bool operator!=(const BaseIterator &a, const BaseIterator &b)
    {
      BLI_assert(a.m_slots == b.m_slots);
      BLI_assert(a.m_total_slots == b.m_total_slots);
      return a.m_current_slot != b.m_current_slot;
    }

    SubIterator begin() const
    {
      for (uint32_t i = 0; i < m_total_slots; i++) {
        if (m_slots[i].is_set()) {
          return SubIterator(m_slots, m_total_slots, i);
        }
      }
      return this->end();
    }

    SubIterator end() const
    {
      return SubIterator(m_slots, m_total_slots, m_total_slots);
    }

    Slot &current_slot() const
    {
      return m_slots[m_current_slot];
    }
  };

  class KeyIterator final : public BaseIterator<KeyIterator> {
   public:
    KeyIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<KeyIterator>(slots, total_slots, current_slot)
    {
    }

    const Key &operator*() const
    {
      return *this->current_slot().key();
    }
  };

  class ValueIterator final : public BaseIterator<ValueIterator> {
   public:
    ValueIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<ValueIterator>(slots, total_slots, current_slot)
    {
    }

    const Value &operator*() const
    {
      return *this->current_slot().value();
    }
  };

  class MutableValueIterator final : public BaseIterator<MutableValueIterator> {
   public:
    MutableValueIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<MutableValueIterator>(slots, total_slots, current_slot)
    {
    }

    Value &operator*()
    {
      return *this->current_slot().value();
    }
  };

  class ItemIterator final : public BaseIterator<ItemIterator> {
   public:
    ItemIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<ItemIterator>(slots, total_slots, current_slot)
    {
    }

    struct Item {
      const Key &key;
      const Value &value;
    };

    Item operator*() const
    {
      const Slot &slot = this->current_slot();
      return {*slot.key(), *slot.value()};
    }
  };

  class MutableItemIterator final : public BaseIterator<MutableItemIterator> {
   public:
    MutableItemIterator(const Slot *slots, uint32_t total_slots, uint32_t current_slot)
        : BaseIterator<MutableItemIterator>(slots, total_slots, current_slot)
    {
    }

    struct Item {
      const Key &key;
      Value &value;
    };

    Item operator*() const
    {
      Slot &slot = this->current_slot();
      return {*slot.key(), *slot.value()};
    }
  };

  KeyIterator keys() const
  {
    return KeyIterator(m_slots.begin(), m_slots.size(), 0);
  }

  ValueIterator values() const
  {
    return ValueIterator(m_slots.begin(), m_slots.size(), 0);
  }

  MutableValueIterator values()
  {
    return MutableValueIterator(m_slots.begin(), m_slots.size(), 0);
  }

  ItemIterator items() const
  {
    return ItemIterator(m_slots.begin(), m_slots.size(), 0);
  }

  MutableItemIterator items()
  {
    return MutableItemIterator(m_slots.begin(), m_slots.size(), 0);
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

  template<typename ForwardKey, typename ForwardValue>
  void add_new__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->contains(key));

    this->ensure_can_add();
    m_set_or_dummy_slots++;

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.is_empty()) {
        slot.set(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        return;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    this->ensure_can_add();

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.is_empty()) {
        slot.set(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        m_set_or_dummy_slots++;
        return true;
      }
      if (slot.contains(std::forward<ForwardKey>(key), hash)) {
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

  template<typename ForwardKey> Value pop__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains(key));

    m_dummy_slots++;

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot.contains(key, hash)) {
        Value value = *slot.value();
        slot.set_to_dummy();
        return value;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename CreateValueF, typename ModifyValueF>
  auto add_or_modify__impl(ForwardKey &&key,
                           const CreateValueF &create_value,
                           const ModifyValueF &modify_value,
                           uint32_t hash) -> decltype(create_value(nullptr))
  {
    using CreateReturnT = decltype(create_value(nullptr));
    using ModifyReturnT = decltype(modify_value(nullptr));
    BLI_STATIC_ASSERT((std::is_same<CreateReturnT, ModifyReturnT>::value),
                      "Both callbacks should return the same type.");

    this->ensure_can_add();

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];

      if (slot.is_empty()) {
        m_set_or_dummy_slots++;
        slot.set_without_value(std::forward<ForwardKey>(key), hash);
        Value *value_ptr = slot.value();
        return create_value(value_ptr);
      }
      if (slot.contains(std::forward<Key>(key), hash)) {
        Value *value_ptr = slot.value();
        return modify_value(value_ptr);
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename CreateValueF>
  Value &lookup_or_add__impl(ForwardKey &&key, const CreateValueF &create_value, uint32_t hash)
  {
    this->ensure_can_add();

    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      Slot &slot = m_slots[slot_index];

      if (slot.is_empty()) {
        slot.set(std::forward<ForwardKey>(key), create_value(), hash);
        m_set_or_dummy_slots++;
        return *slot.value();
      }
      if (slot.contains(std::forward<ForwardKey>(key), hash)) {
        return *slot.value();
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add_override__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    auto create_func = [&](Value *ptr) {
      new (ptr) Value(std::forward<ForwardValue>(value));
      return true;
    };
    auto modify_func = [&](Value *ptr) {
      *ptr = std::forward<ForwardValue>(value);
      return false;
    };
    return this->add_or_modify__impl(
        std::forward<ForwardKey>(key), create_func, modify_func, hash);
  }

  template<typename ForwardKey>
  const Value *lookup_ptr__impl(const ForwardKey &key, uint32_t hash) const
  {
    SLOT_PROBING_BEGIN (hash, m_slot_mask, slot_index) {
      const Slot &slot = m_slots[slot_index];

      if (slot.is_empty()) {
        return nullptr;
      }
      if (slot.contains(key, hash)) {
        return slot.value();
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

template<typename Key, typename Value> class SimpleMapSlot {
 private:
  static constexpr uint8_t s_is_empty = 0;
  static constexpr uint8_t s_is_set = 1;
  static constexpr uint8_t s_is_dummy = 2;

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
    if (m_state == s_is_set) {
      this->key()->~Key();
      this->value()->~Value();
    }
  }

  SimpleMapSlot(const SimpleMapSlot &other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
      new (this->key()) Key(*other.key());
      new (this->value()) Value(*other.value());
    }
  }

  SimpleMapSlot(SimpleMapSlot &&other)
  {
    m_state = other.m_state;
    if (other.m_state == s_is_set) {
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

  bool is_set() const
  {
    return m_state == s_is_set;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  void set_and_destruct_other(SimpleMapSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_state = s_is_set;
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
    if (m_state == s_is_set) {
      return key == *this->key();
    }
    return false;
  }

  template<typename ForwardKey, typename ForwardValue>
  void set(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->is_set());
    this->set_without_value(std::forward<ForwardKey>(key), hash);
    new (this->value()) Value(std::forward<ForwardValue>(value));
  }

  template<typename ForwardKey> void set_without_value(ForwardKey &&key, uint32_t UNUSED(hash))
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
    this->value()->~Value();
  }
};

template<typename Key, typename Value> struct DefaultMapSlot {
  using type = SimpleMapSlot<Key, Value>;
};

}  // namespace BLI

#endif /* __BLI_SINGLE_SLOT_MAP_HH__ */
