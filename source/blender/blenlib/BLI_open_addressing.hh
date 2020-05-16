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
 * This class offers a useful abstraction for other containers that implement hash tables using
 * open addressing. It handles the following aspects:
 *   - Allocation and deallocation of the open addressing array.
 *   - Optional small object optimization.
 *   - Keeps track of how many elements and dummies are in the table.
 *
 * The nice thing about this abstraction is that it does not get in the way of any performance
 * optimizations. The data that is actually stored in the table is still fully defined by the
 * actual hash table implementation.
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

/** \name Constexpr utility functions.
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

class LinearProbingStrategy {
 private:
  uint32_t m_hash;

 public:
  LinearProbingStrategy(uint32_t hash) : m_hash(hash)
  {
  }

  void next()
  {
    m_hash++;
  }

  uint32_t get() const
  {
    return m_hash;
  }

  uint32_t linear_steps() const
  {
    return 1;
  }
};

class QuadraticProbingStrategy {
 private:
  uint32_t m_original_hash;
  uint32_t m_current_hash;
  uint32_t m_iteration;

 public:
  QuadraticProbingStrategy(uint32_t hash)
      : m_original_hash(hash), m_current_hash(hash), m_iteration(1)
  {
  }

  void next()
  {
    m_current_hash = m_original_hash + ((m_iteration * m_iteration + m_iteration) >> 1);
    m_iteration++;
  }

  uint32_t get() const
  {
    return m_current_hash;
  }

  uint32_t linear_steps() const
  {
    return 1;
  }
};

template<uint32_t LinearSteps = 1> class PythonProbingStrategy {
 private:
  uint32_t m_hash;
  uint32_t m_perturb;

 public:
  PythonProbingStrategy(uint32_t hash) : m_hash(hash), m_perturb(hash)
  {
  }

  void next()
  {
    m_perturb >>= 5;
    m_hash = 5 * m_hash + 1 + m_perturb;
  }

  uint32_t get() const
  {
    return m_hash;
  }

  uint32_t linear_steps() const
  {
    return LinearSteps;
  }
};

template<uint32_t LinearSteps = 2> class ShuffleProbingStrategy {
 private:
  uint32_t m_hash;
  uint32_t m_perturb;

 public:
  ShuffleProbingStrategy(uint32_t hash) : m_hash(hash), m_perturb(hash)
  {
  }

  void next()
  {
    if (m_perturb != 0) {
      m_perturb >>= 10;
      m_hash = ((m_hash >> 16) ^ m_hash) * 0x45d9f3b + m_perturb;
    }
    else {
      m_hash = 5 * m_hash + 1;
    }
  }

  uint32_t get() const
  {
    return m_hash;
  }

  uint32_t linear_steps() const
  {
    return LinearSteps;
  }
};

using DefaultProbingStrategy = ShuffleProbingStrategy<>;

// clang-format off

#define SLOT_PROBING_BEGIN(HASH, MASK, R_SLOT_INDEX) \
  ProbingStrategy probing_strategy(HASH); \
  do { \
    uint32_t linear_offset = 0; \
    uint32_t current_hash = probing_strategy.get(); \
    do { \
      uint32_t R_SLOT_INDEX = (current_hash + linear_offset) & MASK;

#define SLOT_PROBING_END() \
    } while (++linear_offset < probing_strategy.linear_steps()); \
    probing_strategy.next(); \
  } while (true)

// clang-format on

class HashTableStats {
 private:
  Vector<uint32_t> m_keys_by_collision_count;
  uint32_t m_total_collisions;
  float m_average_collisions;
  uint32_t m_size;
  uint32_t m_capacity;
  uint32_t m_dummy_amount;
  float m_load_factor;
  float m_dummy_load_factor;
  uint32_t m_size_in_bytes;
  uint32_t m_size_per_slot;

 public:
  /**
   * Requires that the hash table has the following methods:
   * - count_collisions(key) -> uint32_t
   * - size() -> uint32_t
   * - capacity() -> uint32_t
   * - dummy_amount() -> uint32_t
   * - size_in_bytes() -> uint32_t
   */
  template<typename HashTable, typename Keys>
  HashTableStats(const HashTable &hash_table, const Keys &keys)
  {
    m_total_collisions = 0;
    m_size = hash_table.size();
    m_capacity = hash_table.capacity();
    m_dummy_amount = hash_table.dummy_amount();
    m_size_in_bytes = hash_table.size_in_bytes();

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
    m_dummy_load_factor = (float)m_dummy_amount / (float)m_capacity;
    m_size_per_slot = m_size_in_bytes / m_capacity;
  }

  void print(StringRef name = "")
  {
    std::cout << "Collisions stats: " << name << "\n";
    std::cout << "  Total Slots: " << m_capacity << "\n";
    std::cout << "  Used Slots:  " << m_size << " (" << m_load_factor * 100.0f << " %)\n";
    std::cout << "  Dummy Slots: " << m_dummy_amount << " (" << m_dummy_load_factor * 100.0f
              << " %)\n";

    char memory_size_str[15];
    BLI_str_format_byte_unit(memory_size_str, m_size_in_bytes, true);
    std::cout << "  Size: ~" << memory_size_str << "\n";
    std::cout << "  Size per Slot: " << m_size_per_slot << " bytes\n";

    std::cout << "  Average Collisions: " << m_average_collisions << "\n";
    for (uint32_t collision_count : m_keys_by_collision_count.index_range()) {
      std::cout << "  " << collision_count
                << " Collisions: " << m_keys_by_collision_count[collision_count] << "\n";
    }
  }
};

}  // namespace BLI

#endif /* __BLI_OPEN_ADDRESSING_HH__ */
