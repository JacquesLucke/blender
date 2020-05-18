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
/** \name Probing Strategies
 *
 * This section implements different probing strategies. Those can be used by different hash table
 * implementations like BLI::Set and BLI::Map. A probing strategy produces a sequence of values,
 * based on an initial hash value. The hash table implementation is responsible for mapping these
 * values to slot/bucket indices.
 *
 * A probing strategy has to implement the following methods:
 * - Constructor(uint32_t hash): Start a new probing sequence based on the given hash.
 * - get() const -> uint32_t: Get the current value in the sequence.
 * - next() -> void: Update the internal state, so that the next value can be accessed with get().
 * - linear_steps() -> uint32_t: Returns number of linear probing steps that should be done.
 *
 * Using linear probing steps between larger jumps can result in better performance, due to
 * improved cache usage. However, more linear steps can also make things slower when there are many
 * collisions.
 *
 * Every probing strategy has to guarantee, that every possible uint32_t is returned eventually.
 * This is necessary for correctness. If this is not the case, empty slots might not be found in
 * some cases.
 *
 * The SLOT_PROBING_BEGIN and SLOT_PROBING_END macros can be used to implement a loop that iterates
 * over a probing sequence.
 *
 * Probing strategies can be evaluated with many different criterions. Different use cases often
 * have different optimal strategies. Examples:
 * - If the hash function generates a well distributed initial hash value, the constructor should
 *   be as short as possible. This is because the hash value can be used as slot index almost
 *   immediately, without too many collisions.
 * - If the hash function is bad, it can help if the probing strategy remixes the hash value,
 *   before the first slot is accessed.
 * - Different next() methods can remix the hash value in different ways. Depending on which bits
 *   of the hash value contain the most information, different rehashing strategies work best.
 *
 * \{ */

/**
 * The simplest probing strategy. It's bad in most cases, because it produces clusters in the hash
 * table, which result in many collisions. However, if the hash function is very good or the hash
 * table is small, this strategy might even work best.
 */
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
    return UINT32_MAX;
  }
};

/**
 * A slightly adapted quadratic probing strategy. The distance to the original slot increases
 * quadratically. This method also leads to clustering. Another disadvantage is that not all bits
 * of the original hash are used.
 *
 * The distance i*i is not used, because it does not guarantee, that every slot is hit. Instead (i
 * * i + i) / 2 is used.
 *
 * In the first few steps, this strategy can have good cache performance. It largely depends on how
 * many keys fit into a cache line in the hash table.
 */
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

/**
 * This is the probing strategy used by CPython (in 2020).
 *
 * It is very fast when the original hash value is good. If there are collisions, more bits of the
 * hash value are taken into account.
 *
 * LinearSteps: Can be set to something larger than 1 for improved cache performance in some cases.
 * PreShuffle: When true, the initial call to next() will be done to the constructor. This can help
 *   against bad hash functions.
 */
template<uint32_t LinearSteps = 1, bool PreShuffle = false> class PythonProbingStrategy {
 private:
  uint32_t m_hash;
  uint32_t m_perturb;

 public:
  PythonProbingStrategy(uint32_t hash) : m_hash(hash), m_perturb(hash)
  {
    if (PreShuffle) {
      this->next();
    }
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

/**
 * Similar to the Python probing strategy. However, it does a bit more shuffling in the next()
 * method. This way more bits are taken into account earlier. After a couple of collisions (that
 * should happen rarely), it will fallback to a sequence that hits every slot.
 */
template<uint32_t LinearSteps = 2, bool PreShuffle = false> class ShuffleProbingStrategy {
 private:
  uint32_t m_hash;
  uint32_t m_perturb;

 public:
  ShuffleProbingStrategy(uint32_t hash) : m_hash(hash), m_perturb(hash)
  {
    if (PreShuffle) {
      this->next();
    }
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

/**
 * Having a specified default is convenient.
 */
using DefaultProbingStrategy = PythonProbingStrategy<>;

/* Turning off clang format here, because otherwise it will mess up the alignment between the
 * macros. */
// clang-format off

/**
 * Both macros together form a loop that iterates over slot indices in a hash table with a
 * power-of-2 size. The macro assumes that `ProbingStrategy` is a name for a class
 * implementing the probing strategy interface.
 *
 * You must not `break` out of this loop. Only `return` is permitted. If you don't return
 * out of the loop, it will be an infinite loop. These loops should not be nested within the
 * same function.
 *
 * HASH: The initial hash as produced by a hash function.
 * MASK: A bit mask such that (hash & MASK) is a valid slot index.
 * R_SLOT_INDEX: Name of the variable that will contain the slot index.
 */
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
  uint32_t m_dummy_amount;
  float m_load_factor;
  float m_dummy_load_factor;
  uint32_t m_size_per_element;
  uint32_t m_size_in_bytes;

 public:
  /**
   * Requires that the hash table has the following methods:
   * - count_collisions(key) -> uint32_t
   * - size() -> uint32_t
   * - capacity() -> uint32_t
   * - dummy_amount() -> uint32_t
   * - size_per_element() -> uint32_t
   * - size_in_bytes() -> uint32_t
   */
  template<typename HashTable, typename Keys>
  HashTableStats(const HashTable &hash_table, const Keys &keys)
  {
    m_total_collisions = 0;
    m_size = hash_table.size();
    m_capacity = hash_table.capacity();
    m_dummy_amount = hash_table.dummy_amount();
    m_size_per_element = hash_table.size_per_element();
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
