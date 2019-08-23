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
 * This file provides a set implementation that uses open addressing with probing.
 */

#pragma once

#include "BLI_hash.hpp"
#include "BLI_open_addressing.hpp"
#include "BLI_vector.hpp"

namespace BLI {

// clang-format off

#define ITER_SLOTS_BEGIN(VALUE, ARRAY, OPTIONAL_CONST, R_ITEM, R_OFFSET) \
  uint32_t hash = MyHash<T>{}(VALUE); \
  uint32_t perturb = hash; \
  while (true) { \
    uint32_t item_index = (hash & ARRAY.slot_mask()) >> OFFSET_SHIFT; \
    uint8_t R_OFFSET = hash & OFFSET_MASK; \
    uint8_t initial_offset = R_OFFSET; \
    OPTIONAL_CONST Item &R_ITEM = ARRAY.item(item_index); \
    do {

#define ITER_SLOTS_END(R_OFFSET) \
      R_OFFSET = (R_OFFSET + 1) & OFFSET_MASK; \
    } while (R_OFFSET != initial_offset); \
    perturb >>= 5; \
    hash = hash * 5 + 1 + perturb; \
  } ((void)0)

// clang-format on

template<typename T, typename Allocator = GuardedAllocator> class Set {
 private:
  static constexpr uint32_t OFFSET_MASK = 3;
  static constexpr uint32_t OFFSET_SHIFT = 2;

  class Item {
   private:
    static constexpr uint8_t IS_EMPTY = 0;
    static constexpr uint8_t IS_SET = 1;
    static constexpr uint8_t IS_DUMMY = 2;

    uint8_t m_status[4];
    char m_values[4 * sizeof(T)];

   public:
    static constexpr uint32_t slots_per_item = 4;

    Item()
    {
      m_status[0] = IS_EMPTY;
      m_status[1] = IS_EMPTY;
      m_status[2] = IS_EMPTY;
      m_status[3] = IS_EMPTY;
    }

    ~Item()
    {
      for (uint32_t offset = 0; offset < 4; offset++) {
        if (m_status[offset] == IS_SET) {
          destruct(this->value(offset));
        }
      }
    }

    Item(const Item &other)
    {
      for (uint32_t offset = 0; offset < 4; offset++) {
        uint8_t status = other.m_status[offset];
        m_status[offset] = status;
        if (status == IS_SET) {
          T *src = other.value(offset);
          T *dst = this->value(offset);
          new (dst) T(*src);
        }
      }
    }

    Item(Item &&other)
    {
      for (uint32_t offset = 0; offset < 4; offset++) {
        uint8_t status = other.m_status[offset];
        m_status[offset] = status;
        if (status == IS_SET) {
          T *src = other.value(offset);
          T *dst = this->value(offset);
          new (dst) T(std::move(*src));
        }
      }
    }

    Item &operator=(const Item &other) = delete;
    Item &operator=(Item &&other) = delete;

    T *value(uint32_t offset) const
    {
      return (T *)(m_values + offset * sizeof(T));
    }

    void copy_in(uint32_t offset, const T &value)
    {
      BLI_assert(m_status[offset] != IS_SET);
      m_status[offset] = IS_SET;
      T *dst = this->value(offset);
      new (dst) T(value);
    }

    void move_in(uint32_t offset, T &value)
    {
      BLI_assert(m_status[offset] != IS_SET);
      m_status[offset] = IS_SET;
      T *dst = this->value(offset);
      new (dst) T(std::move(value));
    }

    void set_dummy(uint32_t offset)
    {
      BLI_assert(m_status[offset] == IS_SET);
      m_status[offset] = IS_DUMMY;
      destruct(this->value(offset));
    }

    bool is_empty(uint32_t offset) const
    {
      return m_status[offset] == IS_EMPTY;
    }

    bool is_set(uint32_t offset) const
    {
      return m_status[offset] == IS_SET;
    }

    bool is_dummy(uint32_t offset) const
    {
      return m_status[offset] == IS_DUMMY;
    }

    bool has_value(uint32_t offset, const T &value) const
    {
      return m_status[offset] == IS_SET && *this->value(offset) == value;
    }
  };

  using ArrayType = OpenAddressingArray<Item, 1, Allocator>;
  ArrayType m_array = OpenAddressingArray<Item>();

 public:
  Set() = default;

  Set(ArrayRef<T> values)
  {
    for (const T &value : values) {
      this->add(value);
    }
  }

  Set(std::initializer_list<T> values) : Set(ArrayRef<T>(values))
  {
  }

  void reserve(uint32_t min_usable_slots)
  {
    this->grow(min_usable_slots);
  }

  void add_new(const T &value)
  {
    BLI_assert(!this->contains(value));
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (value, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.copy_in(offset, value);
        m_array.update__empty_to_set();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  bool add(const T &value)
  {
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (value, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.copy_in(offset, value);
        m_array.update__empty_to_set();
        return true;
      }
      else if (item.has_value(offset, value)) {
        return false;
      }
    }
    ITER_SLOTS_END(offset);
  }

  void add_multiple(ArrayRef<T> values)
  {
    for (const T &value : values) {
      this->add(value);
    }
  }

  void add_multiple_new(ArrayRef<T> values)
  {
    for (const T &value : values) {
      this->add_new(value);
    }
  }

  bool contains(const T &value) const
  {
    ITER_SLOTS_BEGIN (value, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return false;
      }
      else if (item.has_value(offset, value)) {
        return true;
      }
    }
    ITER_SLOTS_END(offset);
  }

  void remove(const T &value)
  {
    BLI_assert(this->contains(value));
    ITER_SLOTS_BEGIN (value, m_array, , item, offset) {
      if (item.has_value(offset, value)) {
        item.set_dummy(offset);
        m_array.update__set_to_dummy();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  Vector<T> to_small_vector() const
  {
    Vector<T> vector;
    vector.reserve(this->size());
    for (const T &value : *this) {
      vector.append(value);
    }
    return vector;
  }

  uint32_t size() const
  {
    return m_array.slots_set();
  }

  static bool Intersects(const Set &a, const Set &b)
  {
    /* Make sure we iterate over the shorter set. */
    if (a.size() > b.size()) {
      return Intersects(b, a);
    }

    for (const T &value : a) {
      if (b.contains(value)) {
        return true;
      }
    }
    return false;
  }

  static bool Disjoint(const Set &a, const Set &b)
  {
    return !Intersects(a, b);
  }

  void print_table() const
  {
    std::cout << "Hash Table:\n";
    std::cout << "  Size: " << m_array.slots_set() << '\n';
    std::cout << "  Capacity: " << m_array.slots_total() << '\n';
    uint32_t item_index = 0;
    for (const Item &item : m_array) {
      std::cout << "   Item: " << item_index++ << '\n';
      for (uint32_t offset = 0; offset < 4; offset++) {
        std::cout << "    " << offset << " \t";
        if (item.is_empty(offset)) {
          std::cout << "    <empty>\n";
        }
        else if (item.is_set(offset)) {
          const T &value = *item.value(offset);
          uint32_t collisions = this->count_collisions(value);
          std::cout << "    " << value << "  \t Collisions: " << collisions << '\n';
        }
        else if (item.is_dummy(offset)) {
          std::cout << "    <dummy>\n";
        }
      }
    }
  }

  class Iterator {
   private:
    const Set *m_set;
    uint32_t m_slot;

   public:
    Iterator(const Set *set, uint32_t slot) : m_set(set), m_slot(slot)
    {
    }

    Iterator &operator++()
    {
      m_slot = m_set->next_slot(m_slot + 1);
      return *this;
    }

    const T &operator*() const
    {
      uint32_t item_index = m_slot >> OFFSET_SHIFT;
      uint32_t offset = m_slot & OFFSET_MASK;
      const Item &item = m_set->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return *item.value(offset);
    }

    friend bool operator==(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.m_set == b.m_set);
      return a.m_slot == b.m_slot;
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      return !(a == b);
    }
  };

  friend Iterator;

  Iterator begin() const
  {
    return Iterator(this, this->next_slot(0));
  }

  Iterator end() const
  {
    return Iterator(this, m_array.slots_total());
  }

 private:
  uint32_t next_slot(uint32_t slot) const
  {
    for (; slot < m_array.slots_total(); slot++) {
      uint32_t item_index = slot >> OFFSET_SHIFT;
      uint32_t offset = slot & OFFSET_MASK;
      const Item &item = m_array.item(item_index);
      if (item.is_set(offset)) {
        return slot;
      }
    }
    return slot;
  }

  void ensure_can_add()
  {
    if (m_array.should_grow()) {
      this->grow(this->size() + 1);
    }
  }

  void grow(uint32_t min_usable_slots)
  {
    // std::cout << "Grow at " << m_array.slots_set() << '/' << m_array.slots_total() << '\n';
    ArrayType new_array = m_array.init_reserved(min_usable_slots);

    for (Item &old_item : m_array) {
      for (uint8_t offset = 0; offset < 4; offset++) {
        if (old_item.is_set(offset)) {
          this->add_after_grow(*old_item.value(offset), new_array);
        }
      }
    }

    m_array = std::move(new_array);
  }

  void add_after_grow(T &old_value, ArrayType &new_array)
  {
    ITER_SLOTS_BEGIN (old_value, new_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.move_in(offset, old_value);
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  uint32_t count_collisions(const T &value) const
  {
    uint32_t collisions = 0;
    ITER_SLOTS_BEGIN (value, m_array, const, item, offset) {
      if (item.is_empty(offset) || item.has_value(offset, value)) {
        return collisions;
      }
      collisions++;
    }
    ITER_SLOTS_END(offset);
  }
};

#undef ITER_SLOTS_BEGIN
#undef ITER_SLOTS_END

}  // namespace BLI
