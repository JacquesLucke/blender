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

#ifndef __BLI_MAP_HH__
#define __BLI_MAP_HH__

/** \file
 * \ingroup bli
 *
 * A `BLI::Map<Key, Value>` is an unordered container that stores key-value pairs. The keys have to
 * be unique. It is designed to be a more convenient and efficient replacement for
 * `std::unordered_map`. All core operations (add, lookup, remove and contains) can be done in O(1)
 * amortized expected time.
 *
 * In most cases, your default choice for a hash map in Blender should be `BLI::Map`.
 *
 * The implementation uses open addressing in a flat array. The number of slots is always a power
 * of two. More implementation details depend on the used template parameters.
 *
 * Lookup operations with other types than Key can be done using the methods with the suffix "_as".
 * This is commonly used when std::string is used as key, but lookups are done using StringRef. The
 * hash function has to be able to hash those other types as well.
 */

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_probing_strategies.hh"

namespace BLI {

/* This is defined in BLI_map_slots.hh. */
template<typename Key, typename Value> struct DefaultMapSlot;

template<
    /**
     * Type of the keys stored in the map. Keys have to be hashable and movable.
     */
    typename Key,
    /**
     * Type of the value that is stored per key. It has to be movable as well.
     */
    typename Value,
    /**
     * The minimum number of elements that can be stored in this Map without doing a heap
     * allocation. This is useful when you expect to have many small maps. However, keep in mind
     * that (other than in a vector) initializing a map has a O(n) cost in the number of slots.
     *
     * When Key or Value are large, the small buffer optimization is disabled by default to avoid
     * large unexpected allocations on the stack. It can still be enabled explicitely though.
     */
    uint32_t InlineBufferCapacity = (sizeof(Key) + sizeof(Value) < 100) ? 4 : 0,
    /**
     * The strategy used to deal with collistions. They are defined in BLI_probing_strategies.hh.
     */
    typename ProbingStrategy = DefaultProbingStrategy,
    /**
     * The hash function used to hash the keys. There is a default for many types. See BLI_hash.hh
     * for examples on how to define a custom hash function.
     */
    typename Hash = DefaultHash<Key>,
    /**
     * The equality operator used to compare keys. By default it will simply compare keys using the
     * `==` operator.
     */
    typename IsEqual = DefaultEquality<Key>,
    /**
     * This is what will actually be stored in the hash table array. At a minimum a slot has to be
     * able to hold a key, a value and information about whether the slot is empty, occupied or
     * removed. Using a non-standard slot type can improve performance or reduce the memory
     * footprint for some types.
     */
    typename Slot = typename DefaultMapSlot<Key, Value>::type,
    /**
     * The allocator used by this map. Should rarely be changed, except when you don't want that
     * MEM_mallocN etc. is used internally.
     */
    typename Allocator = GuardedAllocator>
class Map {
 private:
  /**
   * Slots are either empty, occupied or removed. The number of occupied slots can be computed by
   * subtracting the removed slots from the occupied-and-removed slots.
   */
  uint32_t m_removed_slots;
  uint32_t m_occupied_and_removed_slots;

  /**
   * The maximum number of slots that can be used (either occupied or removed) until the set has to
   * grow. This is the number of total slots times the max load factor.
   */
  uint32_t m_usable_slots;

  /**
   * The number of slots minus one. This is a bit mask that can be used to turn any integer into a
   * valid slot index efficiently.
   */
  uint32_t m_slot_mask;

#define LOAD_FACTOR 1, 2
  LoadFactor m_max_load_factor = LoadFactor(LOAD_FACTOR);
  using SlotArray =
      Array<Slot, LoadFactor::compute_total_slots(InlineBufferCapacity, LOAD_FACTOR), Allocator>;
#undef LOAD_FACTOR

  /**
   * This is the array that contains the actual slots. There is always at least one empty slot and
   * the size of the array is a power of two.
   */
  SlotArray m_slots;

  /**
   * Iterate over a slot index sequence for a given hash.
   */
#define MAP_SLOT_PROBING_BEGIN(HASH, R_SLOT) \
  SLOT_PROBING_BEGIN (ProbingStrategy, HASH, m_slot_mask, SLOT_INDEX) \
    auto &R_SLOT = m_slots[SLOT_INDEX];
#define MAP_SLOT_PROBING_END() SLOT_PROBING_END()

 public:
  /**
   * Initialize an empty map. This is a cheap operation no matter how large the inline buffer is.
   * This is necessary to avoid a high cost when no elements are added at all. An optimized grow
   * operation is performed on the first insertion.
   */
  Map()
      : m_removed_slots(0),
        m_occupied_and_removed_slots(0),
        m_usable_slots(0),
        m_slot_mask(0),
        m_slots(1)
  {
  }

  ~Map() = default;

  Map(const Map &other) = default;

  Map(Map &&other)
      : m_removed_slots(other.m_removed_slots),
        m_occupied_and_removed_slots(other.m_occupied_and_removed_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask),
        m_slots(std::move(other.m_slots))
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

  /**
   * Insert a new key-value-pair into the map. It is expected that the key is not yet in the map.
   */
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

  /**
   * Add a key-value-pair to the map. If the map contains the key already, nothing is changed.
   * If you want to replace the currently stored value, use `add_overwrite`.
   * Returns true when the key has been newly added.
   *
   * This is similar to std::unordered_map::insert.
   */
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

  /**
   * Adds a key-value-pair to the map. If the map contained the key already, the corresponding
   * value will be replaced.
   * Returns true when the key has been newly added.
   *
   * This is similar to std::unordered_map::insert_or_assign.
   */
  bool add_overwrite(const Key &key, const Value &value)
  {
    return this->add_overwrite_as(key, value);
  }
  bool add_overwrite(const Key &key, Value &&value)
  {
    return this->add_overwrite_as(key, std::move(value));
  }
  bool add_overwrite(Key &&key, const Value &value)
  {
    return this->add_overwrite_as(std::move(key), value);
  }
  bool add_overwrite(Key &&key, Value &&value)
  {
    return this->add_overwrite_as(std::move(key), std::move(value));
  }

  /**
   * Same as `add_overwrite`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> bool add_overwrite_as(ForwardKey &&key, const Value &value)
  {
    return this->add_overwrite__impl(std::forward<ForwardKey>(key), value, Hash{}(key));
  }
  template<typename ForwardKey> bool add_overwrite_as(ForwardKey &&key, Value &&value)
  {
    return this->add_overwrite__impl(std::forward<ForwardKey>(key), std::move(value), Hash{}(key));
  }

  /**
   * Returns true there is a value that corresponds to the given key in the map.
   *
   * This is similar to std::unordered_map::contains.
   */
  bool contains(const Key &key) const
  {
    return this->contains_as(key);
  }

  /**
   * Same as `contains`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> bool contains_as(const ForwardKey &key) const
  {
    return this->contains__impl(key, Hash{}(key));
  }

  /**
   * Deletes the key-value-pair with the given key. This will fail if the key is not in the map
   * beforehand.
   */
  void remove(const Key &key)
  {
    this->remove_as(key);
  }

  /**
   * Same as `remove`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> void remove_as(const ForwardKey &key)
  {
    this->remove__impl(key, Hash{}(key));
  }

  /**
   * Deletes the key-value-pair with the given key. Nothing is done when the key does not exist in
   * the map.
   *
   * This is similar to std::unordered_map::erase.
   */
  bool discard(const Key &key)
  {
    return this->discard_as(key);
  }

  /**
   * Same as `discard`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> bool discard_as(const ForwardKey &key)
  {
    return this->discard__impl(key, Hash{}(key));
  }

  /**
   * Get the value that is stored for the given key and remove it from the map. This will fail if
   * the key is not in the map.
   */
  Value pop(const Key &key)
  {
    return this->pop_as(key);
  }

  /**
   * Same as `pop`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> Value pop_as(const ForwardKey &key)
  {
    return this->pop__impl(key, Hash{}(key));
  }

  /**
   * This method can be used to implement more complex custom behavior without having to make a
   * lookup multiple times.
   *
   * When the key did not yet exist in the map, the create_value function is called. Otherwise the
   * modify_value function is called.
   *
   * Both functions are expected to take a single parameter of type `Value *`. In create_value,
   * this pointer will point to uninitialized memory that has to be initialized by the function. In
   * modify_value, it will point to an already initialized value.
   *
   * The function returns whatever is returned from the create_value or modify_value callback.
   * Therefore, both callbacks have to have the same return type.
   *
   * In this example an integer is stored for every key. The initial value is five and we want to
   * increase it every time the same key is used.
   *   map.add_or_modify(key,
   *                     [](int *value) { *value = 5; },
   *                     [](int *value) { (*value)++; });
   */
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(const Key &key,
                     const CreateValueF &create_value,
                     const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify_as(key, create_value, modify_value);
  }
  template<typename CreateValueF, typename ModifyValueF>
  auto add_or_modify(Key &&key, const CreateValueF &create_value, const ModifyValueF &modify_value)
      -> decltype(create_value(nullptr))
  {
    return this->add_or_modify_as(std::move(key), create_value, modify_value);
  }

  /**
   * Same as `add_or_modify`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename CreateValueF, typename ModifyValueF>
  auto add_or_modify_as(ForwardKey &&key,
                        const CreateValueF &create_value,
                        const ModifyValueF &modify_value) -> decltype(create_value(nullptr))
  {
    return this->add_or_modify__impl(
        std::forward<Key>(key), create_value, modify_value, Hash{}(key));
  }

  /**
   * Returns a pointer to the value that corresponds to the given key. If the key is not in the
   * map, nullptr is returned.
   *
   * This is similar to std::unordered_map::find.
   */
  const Value *lookup_ptr(const Key &key) const
  {
    return this->lookup_ptr_as(key);
  }
  Value *lookup_ptr(const Key &key)
  {
    return this->lookup_ptr_as(key);
  }

  template<typename ForwardKey> const Value *lookup_ptr_as(const ForwardKey &key) const
  {
    return this->lookup_ptr__impl(key, Hash{}(key));
  }
  template<typename ForwardKey> Value *lookup_ptr_as(const ForwardKey &key)
  {
    return const_cast<Value *>(this->lookup_ptr__impl(key, Hash{}(key)));
  }

  /**
   * Returns a reference to the value that corresponds to the given key. This will fail if the key
   * is not in the map.
   */
  const Value &lookup(const Key &key) const
  {
    return this->lookup_as(key);
  }
  Value &lookup(const Key &key)
  {
    return this->lookup_as(key);
  }

  /**
   * Same as `lookup`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey> const Value &lookup_as(const ForwardKey &key) const
  {
    const Value *ptr = this->lookup_ptr_as(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }
  template<typename ForwardKey> Value &lookup_as(const ForwardKey &key)
  {
    Value *ptr = this->lookup_ptr_as(key);
    BLI_assert(ptr != nullptr);
    return *ptr;
  }

  /**
   * Returns a copy of the value that corresponds to the given key. If the key is not in the
   * map, the provided default_value is returned.
   */
  Value lookup_default(const Key &key, const Value &default_value) const
  {
    return this->lookup_default_as(key, default_value);
  }

  /**
   * Same as `lookup_default`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey>
  Value lookup_default_as(const ForwardKey &key, const Value &default_value) const
  {
    const Value *ptr = this->lookup_ptr_as(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return default_value;
    }
  }

  /**
   * Returns a reference to the value that corresponds to the given key. If the key is not yet in
   * the map, it will be newly added.
   *
   * The create_value callback is only called when the key did not exist yet. It is expected to
   * take no parameters and return the value to be inserted.
   */
  template<typename CreateValueF>
  Value &lookup_or_add(const Key &key, const CreateValueF &create_value)
  {
    return this->lookup_or_add_as(key, create_value);
  }
  template<typename CreateValueF> Value &lookup_or_add(Key &&key, const CreateValueF &create_value)
  {
    return this->lookup_or_add_as(std::move(key), create_value);
  }

  /**
   * Same as `lookup_or_add`, but accepts other key types that are supported by the hash function.
   */
  template<typename ForwardKey, typename CreateValueF>
  Value &lookup_or_add_as(ForwardKey &&key, const CreateValueF &create_value)
  {
    return this->lookup_or_add__impl(std::forward<ForwardKey>(key), create_value, Hash{}(key));
  }

  /**
   * Returns a reference to the value that corresponds to the given key. If the key is not yet in
   * the map, it will be newly added. The newly added value will be default constructed.
   *
   */
  Value &lookup_or_add_default(const Key &key)
  {
    return this->lookup_or_add_default_as(key);
  }
  Value &lookup_or_add_default(Key &&key)
  {
    return this->lookup_or_add_default_as(std::move(key));
  }

  /**
   * Same as `lookup_or_add_default`, but accepts other key types that are supported by the hash
   * function.
   */
  template<typename ForwardKey> Value &lookup_or_add_default_as(ForwardKey &&key)
  {
    return this->lookup_or_add(std::forward<ForwardKey>(key), []() { return Value(); });
  }

  /**
   * Calls the provided callback for every key-value-pair in the map. The callback is expected
   * to take a `const Key &` as first and a `const Value &` as second parameter.
   */
  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    uint32_t size = this->size();
    for (uint32_t i = 0; i < size; i++) {
      const Slot &slot = m_slots[i];
      if (slot.is_occupied()) {
        const Key &key = *slot.key();
        const Value &value = *slot.value();
        func(key, value);
      }
    }
  }

  /**
   * A utiltiy iterator that saves code when implementing the actual iterators. This uses
   * the "curiously recurring template pattern" (CRTP).
   */
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
        if (m_slots[m_current_slot].is_occupied()) {
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
        if (m_slots[i].is_occupied()) {
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

  /**
   * Allows writing a range-for loop that iterates over all keys. The iterator is invalidated, when
   * the map is changed.
   */
  KeyIterator keys() const
  {
    return KeyIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all values in the map. The iterator is invalidated, when the map is
   * changed.
   */
  ValueIterator values() const
  {
    return ValueIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all values in the map and allows you to change the values. The
   * iterator is invalidated, when the map is changed.
   */
  MutableValueIterator values()
  {
    return MutableValueIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all key-value-pairs in the map. The key-value-pairs are stored in
   * a temporary struct with a .key and a .value field.
   */
  ItemIterator items() const
  {
    return ItemIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Returns an iterator over all key-value-pairs in the map. The key-value-pairs are stored in
   * a temporary struct with a .key and a .value field.
   *
   * This iterator also allows you to modify the value (but not the key).
   */
  MutableItemIterator items()
  {
    return MutableItemIterator(m_slots.data(), m_slots.size(), 0);
  }

  /**
   * Print common statistics like size and collision count. This is mostly for debugging purposes.
   */
  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, this->keys());
    stats.print();
  }

  /**
   * Return the number of key-value-pairs that are stored in the map.
   */
  uint32_t size() const
  {
    return m_occupied_and_removed_slots - m_removed_slots;
  }

  /**
   * Returns true if there are no elements in the map.
   *
   * This is similar to std::unordered_map::empty.
   */
  bool is_empty() const
  {
    return m_occupied_and_removed_slots == m_removed_slots;
  }

  /**
   * Returns the number of available slots. This is mostly for debugging purposes.
   */
  uint32_t capacity() const
  {
    return m_slots.size();
  }

  /**
   * Returns the amount of removed slots in the set. This is mostly for debugging purposes.
   */
  uint32_t removed_amount() const
  {
    return m_removed_slots;
  }

  /**
   * Returns the bytes required per element. This is mostly for debugging purposes.
   */
  uint32_t size_per_element() const
  {
    return sizeof(Slot);
  }

  /**
   * Returns the approximage memory requirements of the set in bytes. This is more correct for
   * larger sets.
   */
  uint32_t size_in_bytes() const
  {
    return sizeof(Slot) * m_slots.size();
  }

  /**
   * Removes all key-value-pairs from the map.
   */
  void clear()
  {
    this->~Map();
    new (this) Map();
  }

  /**
   * Get the number of collisions that the probing strategy has to go through to find the key or
   * determine that it is not in the map.
   */
  uint32_t count_collisions(const Key &key) const
  {
    return this->count_collisions__impl(key, Hash{}(key));
  }

 private:
  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    uint32_t total_slots, usable_slots;
    m_max_load_factor.compute_total_and_usable_slots(
        SlotArray::inline_buffer_capacity(), min_usable_slots, &total_slots, &usable_slots);
    uint32_t new_slot_mask = total_slots - 1;

    /**
     * Optimize the case when the map was empty beforehand. We can avoid some copies here.
     */
    if (this->size() == 0) {
      m_slots.~Array();
      new (&m_slots) SlotArray(total_slots);
      m_removed_slots = 0;
      m_occupied_and_removed_slots = 0;
      m_usable_slots = usable_slots;
      m_slot_mask = new_slot_mask;
      return;
    }

    SlotArray new_slots(total_slots);

    for (Slot &slot : m_slots) {
      if (slot.is_occupied()) {
        this->add_after_grow_and_destruct_old(slot, new_slots, new_slot_mask);
      }
    }

    /* All occupied slots have been destructed already and empty/removed slots are assumed to be
     * trivially destructible. */
    m_slots.clear_without_destruct();
    m_slots = std::move(new_slots);
    m_occupied_and_removed_slots -= m_removed_slots;
    m_usable_slots = usable_slots;
    m_removed_slots = 0;
    m_slot_mask = new_slot_mask;
  }

  void add_after_grow_and_destruct_old(Slot &old_slot,
                                       SlotArray &new_slots,
                                       uint32_t new_slot_mask)
  {
    uint32_t hash = old_slot.get_hash(Hash());
    SLOT_PROBING_BEGIN (ProbingStrategy, hash, new_slot_mask, slot_index) {
      Slot &slot = new_slots[slot_index];
      if (slot.is_empty()) {
        slot.relocate_occupied_here(old_slot, hash);
        return;
      }
    }
    SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool contains__impl(const ForwardKey &key, uint32_t hash) const
  {
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, IsEqual{}, hash)) {
        return true;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  void add_new__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    BLI_assert(!this->contains_as(key));

    this->ensure_can_add();
    m_occupied_and_removed_slots++;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        return;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
  {
    this->ensure_can_add();

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), std::forward<ForwardValue>(value), hash);
        m_occupied_and_removed_slots++;
        return true;
      }
      if (slot.contains(key, IsEqual{}, hash)) {
        return false;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains_as(key));

    m_removed_slots++;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, IsEqual{}, hash)) {
        slot.remove();
        return;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool discard__impl(const ForwardKey &key, uint32_t hash)
  {
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, IsEqual{}, hash)) {
        slot.remove();
        m_removed_slots++;
        return true;
      }
      if (slot.is_empty()) {
        return false;
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey> Value pop__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains_as(key));

    m_removed_slots++;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, IsEqual{}, hash)) {
        Value value = *slot.value();
        slot.remove();
        return value;
      }
    }
    MAP_SLOT_PROBING_END();
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

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        m_occupied_and_removed_slots++;
        slot.occupy_without_value(std::forward<ForwardKey>(key), hash);
        Value *value_ptr = slot.value();
        return create_value(value_ptr);
      }
      if (slot.contains(key, IsEqual{}, hash)) {
        Value *value_ptr = slot.value();
        return modify_value(value_ptr);
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename CreateValueF>
  Value &lookup_or_add__impl(ForwardKey &&key, const CreateValueF &create_value, uint32_t hash)
  {
    this->ensure_can_add();

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), create_value(), hash);
        m_occupied_and_removed_slots++;
        return *slot.value();
      }
      if (slot.contains(key, IsEqual{}, hash)) {
        return *slot.value();
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey, typename ForwardValue>
  bool add_overwrite__impl(ForwardKey &&key, ForwardValue &&value, uint32_t hash)
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
    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return nullptr;
      }
      if (slot.contains(key, IsEqual{}, hash)) {
        return slot.value();
      }
    }
    MAP_SLOT_PROBING_END();
  }

  template<typename ForwardKey>
  uint32_t count_collisions__impl(const ForwardKey &key, uint32_t hash) const
  {
    uint32_t collisions = 0;

    MAP_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, IsEqual{}, hash)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    MAP_SLOT_PROBING_END();
  }

  void ensure_can_add()
  {
    if (m_occupied_and_removed_slots >= m_usable_slots) {
      this->grow(this->size() + 1);
      BLI_assert(m_occupied_and_removed_slots < m_usable_slots);
    }
  }
};

}  // namespace BLI

#include "BLI_map_slots.hh"

#endif /* __BLI_MAP_HH__ */
