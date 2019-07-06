#pragma once

/* The ArrayLookup allows sharing code between a
 * map and set implementation without hacks
 * (like using an empty value when a set is needed).
 *
 * It is search index for another array. Once build,
 * it allows fast `contains` and `find` calls on that array.
 */

#include <cmath>

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"
#include "BLI_math_bits.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"

#define SLOT_EMPTY -1
#define SLOT_DUMMY -2
#define LOAD_FACTOR 0.6f
#define PERTURB_SHIFT 5

#define ITER_SLOTS(KEY, SLOT, STATE) \
  uint32_t SLOT, SLOT##_perturb; \
  int STATE; \
  for (this->first_slot(KEY, &SLOT, &SLOT##_perturb), STATE = m_map[SLOT];; \
       this->next_slot(&SLOT, &SLOT##_perturb), STATE = m_map[SLOT])

namespace BLI {

template<typename T> inline const T &get_key_from_item(const T &item)
{
  return item;
}

template<typename T> struct ArrayLookupHash {
  uint operator()(const T &v) const noexcept
  {
    return std::hash<T>{}(v);
  }
};

template<typename T> struct ArrayLookupHash<T *> {
  uint operator()(const T *v) const noexcept
  {
    return BLI_ghashutil_ptrhash(v);
  }
};

template<typename Key,
         uint N = 4,
         typename Item = Key,
         const Key &GetKey(const Item &entry) = get_key_from_item,
         typename Hash = ArrayLookupHash<Key>>
class ArrayLookup {
 private:
  static constexpr uint calc_exp(uint n)
  {
    return (n > 0) ? 1 + calc_exp(n >> 1) : 0;
  }

  static const uint N_EXP = calc_exp(N);
  using Mapping = SmallVector<int, (1 << N_EXP)>;

  Mapping m_map;
  uint m_length;
  uint m_dummy_amount;
  uint m_max_used_slots;
  uint32_t m_slot_mask;

 public:
  ArrayLookup()
  {
    this->reset_map(1 << N_EXP);
    m_length = 0;
  }

  bool contains(Item *array, const Key &key) const
  {
    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_EMPTY) {
        return false;
      }
      else if (state == SLOT_DUMMY) {
        continue;
      }
      else if (GetKey(array[state]) == key) {
        return true;
      }
    }
  }

  uint add__no_deleted(Item *array, const Key &key, uint desired_new_index)
  {
    BLI_assert(m_dummy_amount == 0);
    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_EMPTY) {
        this->insert_if_fits_or_grow(array, key, desired_new_index, slot);
        m_length++;
        return desired_new_index;
      }
      else if (GetKey(array[state]) == key) {
        return state;
      }
    }
  }

  uint add(Item *array, const Key &key, uint desired_new_index)
  {
    if (m_dummy_amount == 0) {
      return this->add__no_deleted(array, key, desired_new_index);
    }

    int first_dummy_slot = -1;
    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_EMPTY) {
        if (first_dummy_slot == -1) {
          this->insert_if_fits_or_grow(array, key, desired_new_index, slot);
        }
        else {
          m_map[first_dummy_slot] = desired_new_index;
          m_dummy_amount--;
        }
        m_length++;
        return desired_new_index;
      }
      else if (state == SLOT_DUMMY) {
        if (first_dummy_slot == -1) {
          first_dummy_slot = slot;
        }
        /* Fallback in case there are no empty slots left. */
        if (m_map.size() == m_length + m_dummy_amount) {
          this->ensure_can_add(array);
          this->add(array, key, desired_new_index);
        }
      }
      else if (GetKey(array[state]) == key) {
        return state;
      }
    }
  }

  void add_new(Item *array, uint index)
  {
    this->ensure_can_add(array);
    const Key &key = GetKey(array[index]);
    this->insert_index_for_key(key, index);
    m_length++;
  }

  void update_index(const Key &key, uint old_index, uint new_index)
  {
    ITER_SLOTS (key, slot, state) {
      BLI_assert(state != SLOT_EMPTY);
      if (state == old_index) {
        m_map[slot] = new_index;
        break;
      }
    }
  }

  int find(Item *array, const Key &key) const
  {
    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_EMPTY) {
        return -1;
      }
      else if (state == SLOT_DUMMY) {
        continue;
      }
      else if (GetKey(array[state]) == key) {
        return state;
      }
    }
  }

  void remove(const Key &key, uint index)
  {
    ITER_SLOTS (key, slot, state) {
      BLI_assert(state != SLOT_EMPTY);
      if (state == index) {
        m_map[slot] = SLOT_DUMMY;
        m_length--;
        m_dummy_amount++;
        break;
      }
    }
  }

  uint remove(Item *array, const Key &key)
  {
    BLI_assert(this->contains(array, key));
    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_DUMMY) {
        continue;
      }
      else if (GetKey(array[state]) == key) {
        m_map[slot] = SLOT_DUMMY;
        m_length--;
        m_dummy_amount++;
        return state;
      }
    }
  }

 private:
  inline bool ensure_can_add(Item *array)
  {
    if (LIKELY(m_length + m_dummy_amount < m_max_used_slots)) {
      return false;
    }

    this->reset_map(m_map.size() * 2);
    for (uint i = 0; i < m_length; i++) {
      const Key &key = GetKey(array[i]);
      this->insert_index_for_key__no_dummy(key, i);
    }
    return true;
  }

  void reset_map(uint size)
  {
    BLI_assert(count_bits_i(size) == 1);
    m_map = Mapping(size);
    m_map.fill(SLOT_EMPTY);
    m_max_used_slots = m_map.size() * LOAD_FACTOR;
    m_dummy_amount = 0;
    m_slot_mask = size - 1;
  }

  inline void insert_index_for_key(const Key &key, uint index)
  {
    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_EMPTY) {
        m_map[slot] = index;
        break;
      }
      else if (state == SLOT_DUMMY) {
        m_map[slot] = index;
        m_dummy_amount--;
        break;
      }
    }
  }

  inline void insert_index_for_key__no_dummy(const Key &key, uint index)
  {
    ITER_SLOTS (key, slot, state) {
      BLI_assert(state != SLOT_DUMMY);
      if (state == SLOT_EMPTY) {
        m_map[slot] = index;
        break;
      }
    }
  }

  inline void insert_if_fits_or_grow(Item *array,
                                     const Key &key,
                                     uint index,
                                     uint slot_in_current_map)
  {
    bool map_changed = this->ensure_can_add(array);
    if (map_changed) {
      this->insert_index_for_key(key, index);
    }
    else {
      m_map[slot_in_current_map] = index;
    }
  }

  inline float load_factor() const
  {
    return m_length / (float)m_map.size();
  }

  inline void first_slot(const Key &key, uint32_t *slot, uint32_t *perturb) const
  {
    uint32_t hash_value = Hash{}(key);
    *slot = hash_value & m_slot_mask;
    *perturb = hash_value;
  }

  inline void next_slot(uint32_t *slot, uint32_t *perturb) const
  {
    *slot = m_slot_mask & ((5 * *slot) + 1 + *perturb);
    *perturb >>= PERTURB_SHIFT;
  }

  /* Produce Statistics
   *******************************************/

 private:
  struct LookupStats {
    SmallVector<uint> collisions_amount_distribution;
    uint max_collisions = 0;
    float average_collisions;
  };

  struct KeyLookupStats {
    uint collisions_with_dummies = 0;
    uint collisions_with_other_keys = 0;
    bool found = false;
  };

  KeyLookupStats create_lookup_stats_for_key(Item *array, const Key &key) const
  {
    KeyLookupStats key_stats;

    ITER_SLOTS (key, slot, state) {
      if (state == SLOT_DUMMY) {
        key_stats.collisions_with_dummies++;
      }
      else if (state == SLOT_EMPTY) {
        return key_stats;
      }
      else if (GetKey(array[state]) == key) {
        key_stats.found = true;
        return key_stats;
      }
      else {
        key_stats.collisions_with_other_keys++;
      }
    }
  }

  LookupStats create_lookup_stats(Item *array) const
  {
    LookupStats stats;
    stats.collisions_amount_distribution = SmallVector<uint>(m_map.size());
    stats.collisions_amount_distribution.fill(0);

    uint collisions_sum = 0;

    for (uint i = 0; i < m_length; i++) {
      KeyLookupStats key_stats = this->create_lookup_stats_for_key(array, GetKey(array[i]));
      uint total_collisions = key_stats.collisions_with_dummies +
                              key_stats.collisions_with_other_keys;
      stats.collisions_amount_distribution[total_collisions]++;
      stats.max_collisions = MAX2(stats.max_collisions, total_collisions);
      collisions_sum += total_collisions;
    }
    stats.average_collisions = collisions_sum / (float)m_length;
    return stats;
  }

 public:
  void print_lookup_stats(Item *array) const
  {
    LookupStats stats = this->create_lookup_stats(array);
    std::cout << "Lookup Stats:\n";
    std::cout << "  Stored Keys: " << m_length << "\n";
    std::cout << "  Stored Dummies: " << m_dummy_amount << "\n";
    std::cout << "  Map Size: " << m_map.size() << "\n";
    std::cout << "  Load Factor: " << this->load_factor() << "\n";
    std::cout << "  Average Collisions: " << stats.average_collisions << "\n";
    std::cout << "  Max Lookup Collisions: " << stats.max_collisions << "\n\n";

    for (uint i = 0; i <= stats.max_collisions; i++) {
      std::cout << "  " << i << " collision(s): " << stats.collisions_amount_distribution[i]
                << "\n";
    }
  }
};

} /* namespace BLI */

#undef SLOT_EMPTY
#undef SLOT_DUMMY
#undef LOAD_FACTOR
#undef PERTURB_SHIFT
