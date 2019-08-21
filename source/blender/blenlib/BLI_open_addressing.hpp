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

#pragma once

#include "BLI_utildefines.h"
#include "BLI_memory.hpp"
#include "BLI_math_base.h"
#include "BLI_allocator.hpp"

namespace BLI {

template<typename Item, uint32_t ItemsInSmallStorage = 1, typename Allocator = GuardedAllocator>
class OpenAddressingArray {
 private:
  static constexpr auto slots_per_item = Item::slots_per_item;

  Item *m_items;
  uint32_t m_item_amount;
  uint8_t m_item_exponent;
  uint32_t m_slots_total;
  uint32_t m_slots_set_or_dummy;
  uint32_t m_slots_dummy;
  uint32_t m_slot_mask;
  Allocator m_allocator;
  char m_local_storage[sizeof(Item) * ItemsInSmallStorage];

 public:
  explicit OpenAddressingArray(uint8_t item_exponent = 0)
  {
    m_slots_total = (1 << item_exponent) * slots_per_item;
    m_slots_set_or_dummy = 0;
    m_slots_dummy = 0;
    m_slot_mask = m_slots_total - 1;
    m_item_amount = m_slots_total / slots_per_item;
    m_item_exponent = item_exponent;

    if (m_item_amount <= ItemsInSmallStorage) {
      m_items = this->small_storage();
    }
    else {
      m_items = (Item *)m_allocator.allocate_aligned(
          sizeof(Item) * m_item_amount, std::alignment_of<Item>::value, __func__);
    }

    for (uint32_t i = 0; i < m_item_amount; i++) {
      new (m_items + i) Item();
    }
  }

  ~OpenAddressingArray()
  {
    if (m_items != nullptr) {
      for (uint32_t i = 0; i < m_item_amount; i++) {
        m_items[i].~Item();
      }
      if (!this->is_in_small_storage()) {
        m_allocator.deallocate((void *)m_items);
      }
    }
  }

  OpenAddressingArray(const OpenAddressingArray &other)
  {
    m_slots_total = other.m_slots_total;
    m_slots_set_or_dummy = other.m_slots_set_or_dummy;
    m_slots_dummy = other.m_slots_dummy;
    m_slot_mask = other.m_slot_mask;
    m_item_amount = other.m_item_amount;
    m_item_exponent = other.m_item_exponent;

    if (m_item_amount <= ItemsInSmallStorage) {
      m_items = this->small_storage();
    }
    else {
      m_items = (Item *)m_allocator.allocate_aligned(
          sizeof(Item) * m_item_amount, std::alignment_of<Item>::value, __func__);
    }

    uninitialized_copy_n(other.m_items, m_item_amount, m_items);
  }

  OpenAddressingArray(OpenAddressingArray &&other)
  {
    m_slots_total = other.m_slots_total;
    m_slots_set_or_dummy = other.m_slots_set_or_dummy;
    m_slots_dummy = other.m_slots_dummy;
    m_slot_mask = other.m_slot_mask;
    m_item_amount = other.m_item_amount;
    m_item_exponent = other.m_item_exponent;
    if (other.is_in_small_storage()) {
      m_items = this->small_storage();
      uninitialized_relocate_n(other.m_items, m_item_amount, m_items);
    }
    else {
      m_items = other.m_items;
    }

    other.m_items = nullptr;
    other.~OpenAddressingArray();
    new (&other) OpenAddressingArray();
  }

  OpenAddressingArray &operator=(const OpenAddressingArray &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OpenAddressingArray();
    new (this) OpenAddressingArray(other);
    return *this;
  }

  OpenAddressingArray &operator=(OpenAddressingArray &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OpenAddressingArray();
    new (this) OpenAddressingArray(std::move(other));
    return *this;
  }

  OpenAddressingArray init_reserved(uint32_t min_usable_slots) const
  {
    uint8_t item_exponent = log2_ceil_u(min_usable_slots / slots_per_item + 1) + 1;
    OpenAddressingArray grown(item_exponent);
    grown.m_slots_set_or_dummy = this->slots_set();
    return grown;
  }

  uint32_t slots_total() const
  {
    return m_slots_total;
  }

  uint32_t slots_set() const
  {
    return m_slots_set_or_dummy - m_slots_dummy;
  }

  void update__empty_to_set()
  {
    m_slots_set_or_dummy++;
  }

  void update__dummy_to_set()
  {
    m_slots_dummy--;
  }

  void update__set_to_dummy()
  {
    m_slots_dummy++;
  }

  uint32_t slot_mask() const
  {
    return m_slot_mask;
  }

  const Item &item(uint32_t item_index) const
  {
    return m_items[item_index];
  }

  Item &item(uint32_t item_index)
  {
    return m_items[item_index];
  }

  uint8_t item_exponent() const
  {
    return m_item_exponent;
  }

  uint32_t item_amount() const
  {
    return m_item_amount;
  }

  bool should_grow() const
  {
    return m_slots_set_or_dummy >= m_slots_total / 2;
  }

  Item *begin()
  {
    return m_items;
  }

  Item *end()
  {
    return m_items + m_item_amount;
  }

  const Item *begin() const
  {
    return m_items;
  }

  const Item *end() const
  {
    return m_items + m_item_amount;
  }

 private:
  Item *small_storage() const
  {
    return reinterpret_cast<Item *>((char *)m_local_storage);
  }

  bool is_in_small_storage() const
  {
    return m_items == this->small_storage();
  }
};

}  // namespace BLI
