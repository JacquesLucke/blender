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

/** \file
 * \ingroup bli
 *
 * A `BLI::VectorSet<Key>` is an ordered container for elements of type `Key`. It has the same
 * interface as `BLI::Set` with the following extensions:
 * - The insertion order of keys is maintained as long as no elements are removed.
 * - The keys are stored in a continuous array.
 *
 * All core operations (add, remove and contains) can be done in O(1) amortized expected time.
 *
 * Using a VectorSet instead of a normal Set can be benefitial in any of the following
 * circumstances:
 * - The insertion order is important.
 * - Iteration over all keys has to be fast.
 * - The keys in the set are supposed to be passed to a function that does not have to know that
 *   the keys are stored in a set. With a VectorSet, one can get an ArrayRef containing all keys
 *   without additional copies.
 *
 * The implementation uses open addressing in a flat array. The number of slots is always a power
 * of two. Every slot contains state information and an index into the key array. A slot is either
 * empty, occupied or removed. More implementation details depend on the used template parameters.
 *
 * Possible Improvements:
 * - Small buffer optimization for the keys.
 * - Support lookups using other key types without conversion.
 */

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_hash_tables.hh"
#include "BLI_probing_strategies.hh"

namespace BLI {

/* This is defined in BLI_vector_set_slots.hh. */
template<typename Key> struct DefaultVectorSetSlot;

template<
    /**
     * Type of the elements that are stored in this set. It has to be movable.
     */
    typename Key,
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
     * This is what will actually be stored in the hash table array. At a minimum a slot has to be
     * able to hold an array index and information about whether the slot is empty, occupied or
     * removed. Using a non-standard slot type can improve performance for some types.
     */
    typename Slot = typename DefaultVectorSetSlot<Key>::type,
    /**
     * The allocator used by this set. Should rarely be changed, except when you don't want that
     * MEM_mallocN etc. is used internally.
     */
    typename Allocator = GuardedAllocator>
class VectorSet {
 private:
  /**
   * Specify the max load factor as fraction. We can still try different values like 3/4. I got
   * better performance with some values. I'm not sure yet if this should be exposed as parameter.
   */
#define s_max_load_factor_numerator 1
#define s_max_load_factor_denominator 2
#define s_default_slot_array_size \
  total_slot_amount_for_usable_slots(4, s_max_load_factor_numerator, s_max_load_factor_denominator)

  using SlotArray = Array<Slot, s_default_slot_array_size, Allocator>;

  /**
   * This is the array that contains the actual slots. There is always at least one empty slot and
   * the size of the array is a power of two.
   */
  SlotArray m_slots;

  /**
   * Pointer to an array that contains all keys. The keys are sorted by insertion order as long as
   * no keys are removed. The first ->size() elements in this array are initialized. The capacity
   * of the array is m_usable_slots.
   */
  Key *m_keys;

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

  /**
   * Iterate over a slot index sequence for a given hash.
   */
#define VECTOR_SET_SLOT_PROBING_BEGIN(HASH, R_SLOT) \
  SLOT_PROBING_BEGIN (ProbingStrategy, HASH, m_slot_mask, SLOT_INDEX) \
    auto &R_SLOT = m_slots[SLOT_INDEX];
#define VECTOR_SET_SLOT_PROBING_END() SLOT_PROBING_END()

 public:
  /**
   * Initialize an empty vector set. This is a cheap operation and won't do an allocation. This is
   * necessary to avoid a high cost when no elements are added at all. An optimized grow operation
   * is performed on the first insertion.
   */
  VectorSet() : m_slots(1)
  {
    m_removed_slots = 0;
    m_occupied_and_removed_slots = 0;
    m_usable_slots = 0;
    m_slot_mask = 0;
    m_keys = nullptr;
  }

  /**
   * Construct a vetor set that contains the given keys. Duplicates will be removed automatically.
   */
  VectorSet(const std::initializer_list<Key> &keys) : VectorSet()
  {
    this->add_multiple(keys);
  }

  ~VectorSet()
  {
    destruct_n(m_keys, this->size());
    if (m_keys != nullptr) {
      this->deallocate_keys_array(m_keys);
    }
  }

  VectorSet(const VectorSet &other)
      : m_slots(other.m_slots),
        m_removed_slots(other.m_removed_slots),
        m_occupied_and_removed_slots(other.m_occupied_and_removed_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask)
  {
    m_keys = this->allocate_keys_array(m_usable_slots);
    uninitialized_copy_n(other.m_keys, other.size(), m_keys);
  }

  VectorSet(VectorSet &&other)
      : m_slots(std::move(other.m_slots)),
        m_keys(other.m_keys),
        m_removed_slots(other.m_removed_slots),
        m_occupied_and_removed_slots(other.m_occupied_and_removed_slots),
        m_usable_slots(other.m_usable_slots),
        m_slot_mask(other.m_slot_mask)
  {
    other.m_slots = SlotArray(power_of_2_max_u(s_default_slot_array_size));

    other.m_removed_slots = 0;
    other.m_occupied_and_removed_slots = 0;
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

  /**
   * Returns the number of keys stored in the vector set.
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
    return sizeof(Slot) + sizeof(Key);
  }

  /**
   * Returns the approximage memory requirements of the set in bytes. This is more correct for
   * larger sets.
   */
  uint32_t size_in_bytes() const
  {
    return sizeof(Slot) * m_slots.size() + sizeof(Key) * m_usable_slots;
  }

  /**
   * Potentially resize the vector set such that the specified number of keys can be added without
   * another grow operation.
   */
  void reserve(uint32_t min_usable_slots)
  {
    if (m_usable_slots < min_usable_slots) {
      this->grow(min_usable_slots);
    }
  }

  /**
   * Add a new key to the vector set. This method will fail if the key already exists in the set.
   * When you know for certain that a key is not in the set yet, use this method for better
   * performance. This also expresses the intend better.
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
   * Add a key to the vector set. If the key exists in the set already, nothing is done. The return
   * value is true if the key was newly.
   *
   * This is similar to std::unordered_set::insert.
   */
  void add(const Key &key)
  {
    this->add__impl(key, Hash{}(key));
  }
  void add(Key &&key)
  {
    this->add__impl(std::move(key), Hash{}(key));
  }

  /**
   * Convenience function to add many keys to the vector set at once. Duplicates are removed
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
   * Returns true if the key is in the vector set.
   *
   * This is similar to std::unordered_set::find() != std::unordered_set::end().
   */
  bool contains(const Key &key) const
  {
    return this->contains__impl(key, Hash{}(key));
  }

  /**
   * Deletes the key from the set. This will fail if the key is not in the set beforehand.
   * This might change the order of elements in the vector.
   *
   * This is similar to std::unordered_set::erase.
   */
  void remove(const Key &key)
  {
    this->remove__impl(key, Hash{}(key));
  }

  /**
   * Delete and return a key from the set. This will remove the last element in the vector. The
   * order of the remaining elements in the set is not changed.
   */
  Key pop()
  {
    return this->pop__impl();
  }

  /**
   * Return the location of the key in the vector. It is assumed, that the key is in the vector
   * set. If this is not necessarily the case, use `index_try`.
   */
  uint32_t index(const Key &key) const
  {
    return this->index__impl(key, Hash{}(key));
  }

  /**
   * Return the location of the key in the vector. If the key is not in the set, -1 is returned.
   * If you know for sure that the key is in the set, it is better to use `index` instead.
   */
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

  /**
   * Get the key stored at the given position in the vector.
   */
  const Key &operator[](uint32_t index) const
  {
    BLI_assert(index <= this->size());
    return m_keys[index];
  }

  operator ArrayRef<Key>() const
  {
    return ArrayRef<Key>(m_keys, this->size());
  }

  /**
   * Get an ArrayRef referencing the keys vector. The referenced memory buffer is only valid as
   * long as the vector set is not changed.
   *
   * The keys must not be changed, because this would change their hash value.
   */
  ArrayRef<Key> as_ref() const
  {
    return *this;
  }

  /**
   * Print common statistics like size and collision count. This is mostly for debugging purposes.
   */
  void print_stats(StringRef name = "") const
  {
    HashTableStats stats(*this, this->as_ref());
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

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash, m_keys)) {
        return collisions;
      }
      if (slot.is_empty()) {
        return collisions;
      }
      collisions++;
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

 private:
  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    uint32_t total_slots = total_slot_amount_for_usable_slots(
        min_usable_slots, s_max_load_factor_numerator, s_max_load_factor_denominator);
    uint32_t usable_slots = floor_multiplication_with_fraction(
        total_slots, s_max_load_factor_numerator, s_max_load_factor_denominator);
    uint32_t new_slot_mask = total_slots - 1;

    /* Optimize the case when the set was empty beforehand. We can avoid some copies here. */
    if (this->size() == 0) {
      m_slots.~Array();
      new (&m_slots) SlotArray(total_slots);
      m_removed_slots = 0;
      m_occupied_and_removed_slots = 0;
      m_usable_slots = usable_slots;
      m_slot_mask = new_slot_mask;
      m_keys = this->allocate_keys_array(usable_slots);
      return;
    }

    SlotArray new_slots(total_slots);

    for (Slot &slot : m_slots) {
      if (slot.is_occupied()) {
        this->add_after_grow_and_destruct_old(slot, new_slots, new_slot_mask);
      }
    }

    Key *new_keys = this->allocate_keys_array(usable_slots);
    uninitialized_relocate_n(m_keys, this->size(), new_keys);
    this->deallocate_keys_array(m_keys);

    m_slots.clear_without_destruct();
    m_slots = std::move(new_slots);
    m_keys = new_keys;
    m_occupied_and_removed_slots -= m_removed_slots;
    m_usable_slots = usable_slots;
    m_removed_slots = 0;
    m_slot_mask = new_slot_mask;
  }

  void add_after_grow_and_destruct_old(Slot &old_slot,
                                       SlotArray &new_slots,
                                       uint32_t new_slot_mask)
  {
    const Key &key = m_keys[old_slot.index()];
    uint32_t hash = old_slot.get_hash(key, Hash());

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
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        return false;
      }
      if (slot.contains(key, hash, m_keys)) {
        return true;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void add_new__impl(ForwardKey &&key, uint32_t hash)
  {
    BLI_assert(!this->contains(key));

    this->ensure_can_add();

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        uint32_t index = this->size();
        new (m_keys + index) Key(std::forward<ForwardKey>(key));
        slot.occupy(index, hash);
        m_occupied_and_removed_slots++;
        return;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> bool add__impl(ForwardKey &&key, uint32_t hash)
  {
    this->ensure_can_add();

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.is_empty()) {
        uint32_t index = this->size();
        new (m_keys + index) Key(std::forward<ForwardKey>(key));
        m_occupied_and_removed_slots++;
        slot.occupy(index, hash);
        return true;
      }
      if (slot.contains(key, hash, m_keys)) {
        return false;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> uint32_t index__impl(const ForwardKey &key, uint32_t hash) const
  {
    BLI_assert(this->contains(key));

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash, m_keys)) {
        return slot.index();
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> int32_t index_try__impl(const ForwardKey &key, uint32_t hash) const
  {
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash, m_keys)) {
        return slot.index();
      }
      if (slot.is_empty()) {
        return -1;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  Key pop__impl()
  {
    BLI_assert(this->size() > 0);

    uint32_t index_to_pop = this->size() - 1;
    Key key = std::move(m_keys[index_to_pop]);
    m_keys[index_to_pop].~Key();
    uint32_t hash = Hash{}(key);

    m_removed_slots++;

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.has_index(index_to_pop)) {
        slot.remove();
        return key;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  template<typename ForwardKey> void remove__impl(const ForwardKey &key, uint32_t hash)
  {
    BLI_assert(this->contains(key));

    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.contains(key, hash, m_keys)) {
        uint32_t index_to_remove = slot.index();
        uint32_t size = this->size();
        uint32_t last_element_index = size - 1;

        if (index_to_remove < last_element_index) {
          m_keys[index_to_remove] = std::move(m_keys[last_element_index]);
          this->update_slot_index(m_keys[index_to_remove], last_element_index, index_to_remove);
        }

        m_keys[last_element_index].~Key();
        slot.remove();
        m_removed_slots++;
        return;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  void update_slot_index(const Key &key, uint32_t old_index, uint32_t new_index)
  {
    uint32_t hash = Hash{}(key);
    VECTOR_SET_SLOT_PROBING_BEGIN (hash, slot) {
      if (slot.has_index(old_index)) {
        slot.update_index(new_index);
        return;
      }
    }
    VECTOR_SET_SLOT_PROBING_END();
  }

  void ensure_can_add()
  {
    if (m_occupied_and_removed_slots >= m_usable_slots) {
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

#undef s_max_load_factor_numerator
#undef s_max_load_factor_denominator
#undef s_default_slot_array_size
};

}  // namespace BLI

#include "BLI_vector_set_slots.hh"

#endif /* __BLI_VECTOR_SET_HH__ */
