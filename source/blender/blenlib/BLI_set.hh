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
 * remove and contains) can be done in O(1) amortized expected time.
 *
 * In most cases, your default choice for a hash set in Blender should be `BLI::Set`.
 *
 * The implementation uses open addressing in a flat array. The number of slots is always a power
 * of two. More implementation details depend on the used template parameters.
 *
 * Benchmarking hash tables is hard. There are many things that influence how well a hash table
 * performs. It depends on the hash function, probing strategy, max load factor, element type, slot
 * type and of course the actual distribution of the data. Changing just one of these can make the
 * hash table perform very badly. BLI::Set is designed to be relatively fast in all cases, but it's
 * also quite adjustable and can be optimized for a specific use case.
 *
 * Rudimentary benchmarks can be enabled in BLI_set_test.cc. The results of that benchmark are
 * there as well. The numbers show that in this specific case BLI::Set outperforms
 * std::unordered_set consistently. Usually by a factor between 2-4.
 *
 * Possible Improvements:
 * - Support lookups using other key types without conversion.
 * - Branchless loop over slots in grow function (measured ~10% performance improvement in some
 *   cases).
 * - Optimize add_multiple_(new) with software prefetching (measured up to ~30% performance
 *   improvement in some cases).
 * - Provide an api function to lookup multiple keys and optimize that with software prefetching.
 * - Try different load factors.
 */

#include <type_traits>
#include <unordered_set>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_math_bits.h"
#include "BLI_probing_strategies.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

namespace BLI {

/* This is defined in BLI_set_slots.hh. */
template<typename Key> struct DefaultSetSlot;

template<
    /** Type of the elements that are stored in this set. It has to be movable. */
    typename Key,
    /**
     * The minimum number of elements that can be stored in this Set without doing a heap
     * allocation. This is useful when you expect to have many small sets. However, keep in mind
     * that (other than in a vector) initializing a set has a O(n) cost in the number of slots.
     *
     * When Key is large, the small buffer optimization is disabled by default to avoid large
     * unexpected allocations on the stack. It can still be enabled explicitely though.
     */
    uint32_t InlineBufferCapacity = (sizeof(Key) < 100) ? 4 : 0,
    /**
     * The strategy used to deal with collisions. They are defined in BLI_probing_strategies.hh.
     */
    typename ProbingStrategy = DefaultProbingStrategy,
    /**
     * The hash function used to hash the keys. There is a default for many types. See BLI_hash.hh
     * for examples on how to define a custom hash function.
     */
    typename Hash = DefaultHash<Key>,
    /**
     * This is what will actually be stored in the hash table array. At a minimum a slot has to
     * be able to hold a key and information about whether the slot is empty, occupied or removed.
     * Using a non-standard slot type can improve performance or reduce the memory footprint. For
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
  LoadFactor m_load_factor = LoadFactor(LOAD_FACTOR);
  using SlotArray =
      Array<Slot, LoadFactor::compute_total_slots(InlineBufferCapacity, LOAD_FACTOR), Allocator>;
#undef LOAD_FACTOR

  /**
   * This is the array that contains the actual slots. There is always at least one empty slot and
   * the size of the array is a power of two.
   */
  SlotArray m_slots;

  /** Iterate over a slot index sequence for a given hash. */
#define SET_SLOT_PROBING_BEGIN(HASH, R_SLOT) \
  SLOT_PROBING_BEGIN (ProbingStrategy, HASH, m_slot_mask, SLOT_INDEX) \
    auto &R_SLOT = m_slots[SLOT_INDEX];
#define SET_SLOT_PROBING_END() SLOT_PROBING_END()

 public:
  /**
   * Initialize an empty set. This is a cheap operation no matter how large the inline buffer
   * is. This is necessary to avoid a high cost when no elements are added at all. An optimized
   * grow operation is performed on the first insertion.
   */
  Set() : m_slots(1)
  {
    m_removed_slots = 0;
    m_occupied_and_removed_slots = 0;
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
      : m_removed_slots(other.m_removed_slots),
        m_occupied_and_removed_slots(other.m_occupied_and_removed_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask),
        m_slots(std::move(other.m_slots))
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
    return m_occupied_and_removed_slots - m_removed_slots;
  }

  /**
   * Returns true if no keys are stored.
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
   * know for certain that a key is not in the set yet, use this method for better performance.
   * This also expresses the intend better.
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
    this->remove__impl(key, Hash{}(key));
  }

  /**
   * Deletes the key from the set. Returns true when the key did exist beforehand, otherwise false.
   */
  bool discard(const Key &key)
  {
    return this->discard__impl(key, Hash{}(key));
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
        if (m_slots[m_current_slot].is_occupied()) {
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
      if (m_slots[i].is_occupied()) {
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

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    SET_SLOT_PROBING_END();
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
    uint32_t total_slots, usable_slots;
    m_load_factor.compute_total_and_usable_slots(
        SlotArray::inline_buffer_capacity(), min_usable_slots, &total_slots, &usable_slots);
    uint32_t new_slot_mask = total_slots - 1;

    /**
     * Optimize the case when the set was empty beforehand. We can avoid some copies here.
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

    /* The grown array that we insert the keys into. */
    SlotArray new_slots(total_slots);

    for (Slot &slot : m_slots) {
      if (slot.is_occupied()) {
        this->add_after_grow_and_destruct_old(slot, new_slots, new_slot_mask);
      }
    }

    /* All occupied slots have been destructed already and empty/removed slots are assumed to be
     * trivially destructable. */
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
    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, hash)) {
        return true;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void add_new__impl(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->contains(key));

    this->ensure_can_add();

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), hash);
        m_occupied_and_removed_slots++;
        return;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool add__impl(ForwardKey &&key, uint32_t hash)
  {
    this->ensure_can_add();

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        slot.occupy(std::forward<ForwardKey>(key), hash);
        m_occupied_and_removed_slots++;
        return true;
      }
      if (slot.contains(key, hash)) {
        return false;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains(key));
    m_removed_slots++;

    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash)) {
        slot.remove();
        return;
      }
    }
    SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool discard__impl(const ForwardKey &key, uint32_t hash)
  {
    SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash)) {
        slot.remove();
        m_removed_slots++;
        return true;
      }
      if (slot.is_empty()) {
        return false;
      }
    }
    SET_SLOT_PROBING_END();
  }

  void ensure_can_add()
  {
    if (m_occupied_and_removed_slots >= m_usable_slots) {
      this->grow(this->size() + 1);
      BLI_assert(m_occupied_and_removed_slots < m_usable_slots);
    }
  }
};

/**
 * A wrapper for std::unordered_set with the API of BLI::Set. This can be used for benchmarking.
 */
template<typename Key> class StdUnorderedSetWrapper {
 private:
  using SetType = std::unordered_set<Key, BLI::DefaultHash<Key>>;
  SetType m_set;

 public:
  uint32_t size() const
  {
    return (uint32_t)m_set.size();
  }

  bool is_empty() const
  {
    return m_set.empty();
  }

  void reserve(uint32_t n)
  {
    m_set.reserve(n);
  }

  void add_new(const Key &key)
  {
    m_set.insert(key);
  }
  void add_new(Key &&key)
  {
    m_set.insert(std::move(key));
  }

  bool add(const Key &key)
  {
    return m_set.insert(key).second;
  }
  bool add(Key &&key)
  {
    return m_set.insert(std::move(key)).second;
  }

  void add_multiple(ArrayRef<Key> keys)
  {
    for (const Key &key : keys) {
      m_set.insert(key);
    }
  }

  bool contains(const Key &key) const
  {
    return m_set.find(key) != m_set.end();
  }

  void remove(const Key &key)
  {
    m_set.erase(key);
  }

  bool discard(const Key &key)
  {
    return (bool)m_set.erase(key);
  }

  void clear()
  {
    m_set.clear();
  }

  typename SetType::iterator begin() const
  {
    return m_set.begin();
  }

  typename SetType::iterator end() const
  {
    return m_set.end();
  }
};

}  // namespace BLI

#include "BLI_set_slots.hh"

#endif /* __BLI_SET_HH__ */
