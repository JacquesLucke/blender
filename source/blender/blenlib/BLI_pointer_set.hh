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

#ifndef __BLI_POINTER_SET_HH__
#define __BLI_POINTER_SET_HH__

#include "BLI_open_addressing.hh"

namespace BLI {

// clang-format off

#define ITER_SLOTS_BEGIN(PTR, SLOT_MASK, SLOTS, R_SLOT) \
  uint32_t hash = DefaultHash<T *>{}(PTR); \
  uint32_t perturb = hash; \
  while (true) { \
    uint32_t R_INDEX = hash & SLOT_MASK; \
    uint32_t end_index = start_index + 4; \
    do { \
      auto &R_SLOT = SLOTS[R_INDEX];

#define ITER_SLOTS_END() \
    } while (++R_INDEX == end_index); \
    perturb >>= 5; \
    hash = hash * 5 + 1 + perturb; \
  } ((void)0)

// clang-format on

template<typename T, uint InlineBufferCapacity = 0, typename Allocator = GuardedAllocator>
class PointerSet {
 private:
  class Slot {
   private:
    static constexpr uintptr_t IS_EMPTY = 0;
    static constexpr uintptr_t IS_DUMMY = 1;
    static constexpr uintptr_t IS_OVERSHOOT = 2;
    static constexpr uintptr_t MAX_SPECIAL_VALUE = IS_OVERSHOOT;

    uintptr_t m_value;

   public:
    Slot()
    {
      m_value = IS_EMPTY;
    }

    bool is_empty() const
    {
      return m_value == IS_EMPTY;
    }

    bool is_dummy() const
    {
      return m_value == IS_DUMMY;
    }

    bool is_set() const
    {
      return m_value > MAX_SPECIAL_VALUE;
    }

    bool has_value(const T *ptr) const
    {
      BLI_assert(this->is_valid_pointer(ptr));
      return m_value == (uintptr_t)ptr;
    }

    void store(const T *ptr) const
    {
      BLI_assert(this->is_valid_pointer(ptr));
      m_value = (uintptr_t)ptr;
    }

    T *value() const
    {
      return (T *)m_value;
    }

   private:
    static bool is_valid_pointer(const T *ptr)
    {
      uintptr_t value = (uintptr_t)ptr;
      return value > MAX_SPECIAL_VALUE;
    }
  };

  static constexpr uint32_t s_max_load_factor_numerator = 1;
  static constexpr uint32_t s_max_load_factor_denominator = 2;
  static constexpr uint32_t s_chunk_size = 4;

  static constexpr uint32_t compute_number_of_slots_to_allocate(uint32_t min_usable_slots)
  {
    return next_power_of_2_constexpr(compute_min_required_items(
               min_usable_slots, 1, s_max_load_factor_numerator, s_max_load_factor_denominator)) +
           s_chunk_size - 1;
  }

  static constexpr uint32_t s_slots_in_inline_buffer = compute_number_of_slots_to_allocate(
      InlineBufferCapacity);

  using ArrayType = Array<Slot, s_slots_in_inline_buffer, Allocator>;
  ArrayType m_slots;
  uint32_t m_slots_set_or_dummy;
  uint32_t m_slots_usable;
  uint32_t m_slots_dummy;

 public:
  uint size() const
  {
    return m_slots_set_or_dummy - m_slots_dummy;
  }

 private:
  void ensure_can_add()
  {
    if (UNLIKELY(m_slots_set_or_dummy >= m_slots_usable)) {
      this->grow(this->size() + 1);
    }
  }

  BLI_NOINLINE void grow(uint32_t min_usable_slots)
  {
    ArrayType new_slots(compute_number_of_slots_to_allocate(min_usable_slots));

    for (Slot &slot : m_slots) {
      if (slot.is_set()) {
        this->add_after_grow(slot.value(), new_slots, )
      }
    }
  }

  void add_after_grow(const T *ptr, ArrayType &new_slots, uint32_t slot_mask)
  {
    ITER_SLOTS_BEGIN (ptr, slot_mask, new_slots, slot) {
      if (slot.is_empty()) {
        slot.store(ptr);
        return;
      }
    }
    ITER_SLOTS_END();
  }
};

#undef ITER_SLOTS_BEGIN
#undef ITER_SLOTS_END

}  // namespace BLI

#endif /* __BLI_POINTER_SET_HH__ */
