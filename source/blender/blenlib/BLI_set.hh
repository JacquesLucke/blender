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
 * The implementation uses open addressing in a flat array. The number of slots is always a power
 * of two. More implementation details depend on the used template parameters.
 */

#include <type_traits>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_math_bits.h"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

namespace BLI {

/* This is defined in BLI_set_slots.hh. */
template<typename Key> struct DefaultSetSlot;

template<
    /**
     * Type of of the elements that are stored in this set. It has to be movable.
     */
    typename Key,
    /**
     * The minimum number of elements that can be stored in this Set without doing a heap
     * allocation. This is useful when you expect to have many small sets. However, keep in mind
     * that (other than in a vector) initializing a set has a O(n) cost in the number of elements
     * that will be stored.
     */
    uint32_t InlineBufferCapacity = 4,
    /**
     * The strategy used to deal with collisions. They are defined in BLI_hash_tables.hh.
     */
    typename ProbingStrategy = DefaultProbingStrategy,
    /**
     * The hash function used to hash the keys. There is a default for many types. See BLI_hash.hh
     * for examples on how to define a custom hash function.
     */
    typename Hash = DefaultHash<Key>,
    /**
     * This is what will actually be stored in the hash table array. At a minimum a slot has to
     * be able to hold a key and information about whether the slot is empty, set or a dummy. Using
     * a non-standard slot type can improve performance or reduce the memory footprint. For
     * example, a hash can be stored in the slot, to make inequality checks more efficient. Some
     * types have special values that can represent an empty or dummy state, eliminating the need
     * for an additional variable.
     */
    typename Slot = typename DefaultSetSlot<Key>::type,
    /**
     * The allocator used by this set. Should rarely be changed, except when you don't want that
     * MEM_mallocN etc. is used internally.
     */
    typename Allocator = GuardedAllocator>
class Set {
 private:
  /**
   * Specify the max load factor as fraction. We can still try different values like 3/4. I got
   * better performance with some values. I'm not sure yet if this should be exposed as parameter.
   */
  static constexpr uint32_t s_max_load_factor_numerator = 1;
  static constexpr uint32_t s_max_load_factor_denominator = 2;
  static constexpr uint32_t s_inline_slots_capacity = total_slot_amount_for_usable_slots(
      InlineBufferCapacity, s_max_load_factor_numerator, s_max_load_factor_denominator);

  using SlotArray = Array<Slot, s_inline_slots_capacity, Allocator>;

  /**
   * This is the array that contains the actual slots. There is always at least one slot and the
   * size of the array is a power of two.
   */
  SlotArray m_slots;

  /**
   * Slots are either empty, set or dummy. The number of set slots can be computed by subtracting
   * the dummy slots from the set-or-dummy slots.
   */
  uint32_t m_dummy_slots;
  uint32_t m_set_or_dummy_slots;

  /**
   * The maximum number of slots that can be used (either set or dummy) until the set has to grow.
   * This is the number of total slots times the max load factor.
   */
  uint32_t m_usable_slots;

  /**
   * The number of slots minus one. This is a bit mask that can be used to turn any integer into a
   * valid slot index efficiently.
   */
  uint32_t m_slot_mask;

 public:
  /**
   * Initialize an empty set. The number of reserved slots depends on the InlineBufferCapacity
   * parameter.
   */
  Set()
  {
    m_slots = SlotArray(1);

    m_dummy_slots = 0;
    m_set_or_dummy_slots = 0;
    m_usable_slots = 0;
    m_slot_mask = 0;
  }

  ~Set() = default;

  /**
   * Construct a set that contains the given keys. Duplicates will be removed automatically.
   */
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

  /**
   * Returns the number of keys stored in the set.
   */
  uint32_t size() const
  {
    return m_set_or_dummy_slots - m_dummy_slots;
  }

  /**
   * Returns true if no keys are stored.
   */
  bool is_empty() const
  {
    return m_set_or_dummy_slots == m_dummy_slots;
  }

  /**
   * Returns the number of available slots. This is mostly for debugging purposes.
   */
  uint32_t capacity() const
  {
    return m_slots.size();
  }

  /**
   * Returns the amount of dummy slots in the set. This is mostly for debugging purposes.
   */
  uint32_t dummy_amount() const
  {
    return m_dummy_slots;
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
   * Potentially resize the set such that the specified number of keys can be added without another
   * grow operation.
   */
  void reserve(uint32_t min_usable_slots)
  {
    if (m_usable_slots < min_usable_slots) {
      this->grow(min_usable_slots);
    }
  }

  /**
   * Add a new key to the set. This method will fail if the key already exists in the set. When you
   * know for certain that a key is not in the set yet, use this method for better performance and
   * intend specification.
   */
  void add_new(const Key &key)
  {
    this->add_new__impl(key, Hash{}(key));
  }
  void add_new(Key &&key)
  {
    this->add_new__impl(std::move(key), Hash{}(key));
  }

  /**
   * Add a key to the set. If the key exists in the set already, nothing is done. The return value
   * is true if the key was newly added to the set.
   *
   * This is similar to std::unordered_set::insert.
   */
  bool add(const Key &key)
  {
    return this->add__impl(key, Hash{}(key));
  }
  bool add(Key &&key)
  {
    return this->add__impl(std::move(key), Hash{}(key));
  }

  /**
   * Convenience function to add many keys to the set at once. Duplicates are removed
   * automatically.
   *
   * We might be able to make this faster than sequentially adding all keys, but that is not
   * implemented yet.
   */
  void add_multiple(ArrayRef<Key> keys)
  {
    for (const Key &key : keys) {
      this->add(key);
    }
  }

  /**
   * Convenience function to add many new keys to the set at once. The keys must not exist in the
   * set before and there must not be duplicates in the array.
   */
  void add_multiple_new(ArrayRef<Key> keys)
  {
    for (const Key &key : keys) {
      this->add_new(key);
    }
  }

  /**
   * Returns true if the key is in the set.
   *
   * This is similar to std::unordered_set::find() != std::unordered_set::end().
   */
  bool contains(const Key &key) const
  {
    return this->contains__impl(key, Hash{}(key));
  }

  /**
   * Deletes the key from the set. This will fail if the key is not in the set beforehand.
   *
   * This is similar to std::unordered_set::erase.
   */
  void remove(const Key &key)
  {
    return this->remove__impl(key, Hash{}(key));
  }

  /**
   * An iterator that can iterate over all keys in the set. The iterator is invalidated when the
   * set is moved or when it is grown.
   *
   * Keys returned by this iterator are always const. They should not change, since this might also
   * change their hash.
   */
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

  /**
   * Print common statistics like size and collision count. This is mostly for debugging purposes.
   */
  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, *this);
    stats.print();
  }

  /**
   * Get the number of collisions that the probing strategy has to go through to find the key or
   * determine that it is not in the set.
   */
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

  /**
   * Remove all elements from the set.
   */
  void clear()
  {
    this->~Set();
    new (this) Set();
  }

  /**
   * Creates a new slot array and reinserts all keys inside of that. This method can be used to get
   * rid of dummy slots. Also this is useful for benchmarking the grow function.
   */
  void rehash()
  {
    this->grow(this->size());
  }

  /**
   * Returns true if there is a key that exists in both sets.
   */
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

  /**
   * Returns true if no key from a is also in b and vice versa.
   */
  static bool Disjoint(const Set &a, const Set &b)
  {
    return !Intersects(a, b);
  }

 private:
  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    uint32_t total_slots = total_slot_amount_for_usable_slots(
        min_usable_slots, s_max_load_factor_numerator, s_max_load_factor_denominator);
    total_slots = std::max(total_slots, s_inline_slots_capacity);
    uint32_t usable_slots = floor_multiplication_with_fraction(
        total_slots, s_max_load_factor_numerator, s_max_load_factor_denominator);
    uint32_t new_slot_mask = total_slots - 1;

    /**
     * Optimize the case when the set was empty beforehand. We can avoid some copies here.
     */
    if (this->size() == 0) {
      m_slots.~Array();
      new (&m_slots) SlotArray(total_slots);
      m_dummy_slots = 0;
      m_set_or_dummy_slots = 0;
      m_usable_slots = usable_slots;
      m_slot_mask = new_slot_mask;
      return;
    }

    /* The grown array that we insert the keys into. */
    SlotArray new_slots(total_slots);

    uint32_t old_total_slots = m_slots.size();

    if (old_total_slots <= 8192) {
      /**
       * Depending on the distribution of the keys, the branch below is hard to predict for the
       * CPU. Therefore there is a more branch-less version below that has the same semantics.
       * I found an up to 10% grow performance improvement using the branch-less variant. I still
       * have to determine a better check for which algorithm will work better.
       */
      for (Slot &slot : m_slots) {
        if (slot.is_set()) {
          this->add_after_grow_and_destruct_old(slot, new_slots, new_slot_mask);
        }
      }
    }
    else {
      /**
       * A branch-less version of the loop above. Slots are processed in chunks of size 32. The
       * trick is to create a bit mask that contains a one for the slots that are set and a zero
       * otherwise. Then we can use compiler intrinsics to iterate over the 1-bits.
       */
      for (uint32_t chunk_start = 0; chunk_start < old_total_slots; chunk_start += 32) {
        uint32_t set_slot_mask = 0;
        uint32_t chunk_end = chunk_start + 32;
        for (uint32_t i = chunk_start; i < chunk_end; i++) {
          set_slot_mask <<= 1;
          set_slot_mask |= m_slots[i].is_set();
        }
        while (set_slot_mask) {
          uint32_t i = bitscan_reverse_clear_uint(&set_slot_mask);
          this->add_after_grow_and_destruct_old(
              m_slots[chunk_start + i], new_slots, new_slot_mask);
        }
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

}  // namespace BLI

#include "BLI_set_slots.hh"

#endif /* __BLI_SET_HH__ */
