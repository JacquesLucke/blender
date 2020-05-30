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

#ifndef __BLI_OPEN_ADDRESSING_HH__
#define __BLI_OPEN_ADDRESSING_HH__

/** \file
 * \ingroup bli
 *
 * This file contains code that can be shared between different hash table implementations.
 */

#include <cmath>

#include "BLI_allocator.hh"
#include "BLI_array.hh"
#include "BLI_math_base.h"
#include "BLI_memory_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

namespace BLI {

/* -------------------------------------------------------------------- */
/** \name Constexpr Utility Functions
 *
 * Those should eventually be deduplicated with functions in BLI_math_base.h.
 * \{ */

inline constexpr int is_power_of_2_i_constexpr(int n)
{
  return (n & (n - 1)) == 0;
}

inline constexpr uint32_t log2_floor_u_constexpr(uint32_t x)
{
  return x <= 1 ? 0 : 1 + log2_floor_u_constexpr(x >> 1);
}

inline constexpr uint32_t log2_ceil_u_constexpr(uint32_t x)
{
  return (is_power_of_2_i_constexpr((int)x)) ? log2_floor_u_constexpr(x) :
                                               log2_floor_u_constexpr(x) + 1;
}

inline constexpr uint32_t power_of_2_max_u_constexpr(uint32_t x)
{
  return 1 << log2_ceil_u_constexpr(x);
}

template<typename IntT> inline constexpr IntT ceil_division(IntT x, IntT y)
{
  BLI_STATIC_ASSERT(!std::is_signed<IntT>::value, "");
  return x / y + ((x % y) != 0);
}

template<typename IntT> inline constexpr IntT floor_division(IntT x, IntT y)
{
  BLI_STATIC_ASSERT(!std::is_signed<IntT>::value, "");
  return x / y;
}

inline constexpr uint32_t ceil_division_by_fraction(uint32_t x,
                                                    uint32_t numerator,
                                                    uint32_t denominator)
{
  return (uint32_t)ceil_division((uint64_t)x * (uint64_t)denominator, (uint64_t)numerator);
}

inline constexpr uint32_t floor_multiplication_with_fraction(uint32_t x,
                                                             uint32_t numerator,
                                                             uint32_t denominator)
{
  return (uint32_t)((uint64_t)x * (uint64_t)numerator / (uint64_t)denominator);
}

inline constexpr uint32_t total_slot_amount_for_usable_slots(uint32_t min_usable_slots,
                                                             uint32_t max_load_factor_numerator,
                                                             uint32_t max_load_factor_denominator)
{
  return power_of_2_max_u_constexpr(ceil_division_by_fraction(
      min_usable_slots, max_load_factor_numerator, max_load_factor_denominator));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hash Table Stats
 *
 * A utility class that makes it easier for hash table implementations to provide statistics to the
 * developer. These statistics can be helpful when trying to figure out why a hash table is slow.
 *
 * To use this utility, a hash table has to implement various methods, that are mentioned below.
 *
 * \{ */

class HashTableStats {
 private:
  Vector<uint32_t> m_keys_by_collision_count;
  uint32_t m_total_collisions;
  float m_average_collisions;
  uint32_t m_size;
  uint32_t m_capacity;
  uint32_t m_removed_amount;
  float m_load_factor;
  float m_removed_load_factor;
  uint32_t m_size_per_element;
  uint32_t m_size_in_bytes;
  const void *m_address;

 public:
  /**
   * Requires that the hash table has the following methods:
   * - count_collisions(key) -> uint32_t
   * - size() -> uint32_t
   * - capacity() -> uint32_t
   * - removed_amount() -> uint32_t
   * - size_per_element() -> uint32_t
   * - size_in_bytes() -> uint32_t
   */
  template<typename HashTable, typename Keys>
  HashTableStats(const HashTable &hash_table, const Keys &keys)
  {
    m_total_collisions = 0;
    m_size = hash_table.size();
    m_capacity = hash_table.capacity();
    m_removed_amount = hash_table.removed_amount();
    m_size_per_element = hash_table.size_per_element();
    m_size_in_bytes = hash_table.size_in_bytes();
    m_address = (const void *)hash_table;

    for (const auto &key : keys) {
      uint32_t collisions = hash_table.count_collisions(key);
      if (m_keys_by_collision_count.size() <= collisions) {
        m_keys_by_collision_count.append_n_times(
            0, collisions - m_keys_by_collision_count.size() + 1);
      }
      m_keys_by_collision_count[collisions]++;
      m_total_collisions += collisions;
    }

    m_average_collisions = (m_size == 0) ? 0 : (float)m_total_collisions / (float)m_size;
    m_load_factor = (float)m_size / (float)m_capacity;
    m_removed_load_factor = (float)m_removed_amount / (float)m_capacity;
  }

  void print(StringRef name = "")
  {
    std::cout << "Hash Table Stats: " << name << "\n";
    std::cout << "  Address: " << m_address << "\n";
    std::cout << "  Total Slots: " << m_capacity << "\n";
    std::cout << "  Occupied Slots:  " << m_size << " (" << m_load_factor * 100.0f << " %)\n";
    std::cout << "  Removed Slots: " << m_removed_amount << " (" << m_removed_load_factor * 100.0f
              << " %)\n";

    char memory_size_str[15];
    BLI_str_format_byte_unit(memory_size_str, m_size_in_bytes, true);
    std::cout << "  Size: ~" << memory_size_str << "\n";
    std::cout << "  Size per Slot: " << m_size_per_element << " bytes\n";

    std::cout << "  Average Collisions: " << m_average_collisions << "\n";
    for (uint32_t collision_count : m_keys_by_collision_count.index_range()) {
      std::cout << "  " << collision_count
                << " Collisions: " << m_keys_by_collision_count[collision_count] << "\n";
    }
  }
};

/** \} */

}  // namespace BLI

#endif /* __BLI_OPEN_ADDRESSING_HH__ */
