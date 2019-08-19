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
 * An unordered map implementation with small object optimization.
 * Similar to Set, this builds on top of Vector
 * and ArrayLookup to reduce what this code has to deal with.
 */

#pragma once

#include "BLI_hash.hpp"
#include "BLI_array_ref.hpp"
#include "BLI_open_addressing.hpp"

namespace BLI {

template<typename KeyT, typename ValueT, typename Allocator = GuardedAllocator> class Map {
 private:
  static constexpr uint32_t OFFSET_MASK = 3;
  static constexpr uint8_t IS_EMPTY = 0;
  static constexpr uint8_t IS_SET = 1;
  static constexpr uint8_t IS_DUMMY = 2;

  class Item {
   private:
    uint8_t m_status[4];
    char m_keys[4 * sizeof(KeyT)];
    char m_values[4 * sizeof(ValueT)];

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
          this->key(offset)->~KeyT();
          this->value(offset)->~ValueT();
        }
      }
    }

    Item(const Item &other)
    {
      for (uint32_t offset = 0; offset < 4; offset++) {
        uint8_t status = other.m_status[offset];
        m_status[offset] = status;
        if (status == IS_SET) {
          new (this->key(offset)) KeyT(*other.key(offset));
          new (this->value(offset)) ValueT(*other.value(offset));
        }
      }
    }

    Item(Item &&other)
    {
      for (uint32_t offset = 0; offset < 4; offset++) {
        uint8_t status = other.m_status[offset];
        m_status[offset] = status;
        if (status == IS_SET) {
          new (this->key(offset)) KeyT(std::move(*other.key(offset)));
          new (this->value(offset)) ValueT(std::move(*other.value(offset)));
        }
      }
    }

    bool has_key(uint32_t offset, const KeyT &key) const
    {
      return m_status[offset] == IS_SET && key == *this->key(offset);
    }

    bool is_set(uint32_t offset) const
    {
      return m_status[offset] == IS_SET;
    }

    bool is_empty(uint32_t offset) const
    {
      return m_status[offset] == IS_EMPTY;
    }

    KeyT *key(uint32_t offset) const
    {
      return (KeyT *)(m_keys + offset * sizeof(KeyT));
    }

    ValueT *value(uint32_t offset) const
    {
      return (ValueT *)(m_values + offset * sizeof(ValueT));
    }

    uint8_t status(uint32_t offset) const
    {
      return m_status[offset];
    }

    void copy_in(uint32_t offset, const KeyT &key, const ValueT &value)
    {
      BLI_assert(m_status[offset] != IS_SET);
      m_status[offset] = IS_SET;
      new (this->key(offset)) KeyT(key);
      new (this->value(offset)) ValueT(value);
    }

    void move_in(uint32_t offset, KeyT &key, ValueT &value)
    {
      BLI_assert(m_status[offset] != IS_SET);
      m_status[offset] = IS_SET;
      new (this->key(offset)) KeyT(std::move(key));
      new (this->value(offset)) ValueT(std::move(value));
    }

    void set_dummy(uint32_t offset)
    {
      BLI_assert(m_status[offset] == IS_SET);
      m_status[offset] = IS_DUMMY;
      destruct(this->key(offset));
      destruct(this->value(offset));
    }
  };

  using ArrayType = OpenAddressingArray<Item, 1, Allocator>;
  ArrayType m_array;

 public:
  Map() = default;

  // clang-format off

#define ITER_SLOTS_BEGIN(KEY, ARRAY, OPTIONAL_CONST, R_ITEM, R_OFFSET) \
  uint32_t hash = MyHash<KeyT>{}(KEY); \
  uint32_t perturb = hash; \
  while (true) { \
    uint32_t item_index = (hash & ARRAY.slot_mask()) >> 2; \
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

  void add_new(const KeyT &key, const ValueT &value)
  {
    BLI_assert(!this->contains(key));
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.status(offset) == IS_EMPTY) {
        item.copy_in(offset, key, value);
        m_array.update__empty_to_set();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  bool add(const KeyT &key, const ValueT &value)
  {
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.status(offset) == IS_EMPTY) {
        item.copy_in(offset, key, value);
        m_array.update__empty_to_set();
        return true;
      }
      else if (item.has_key(offset, key)) {
        return false;
      }
    }
    ITER_SLOTS_END(offset);
  }

  void remove(const KeyT &key)
  {
    BLI_assert(this->contains(key));
    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.has_key(offset, key)) {
        item.set_dummy(offset);
        m_array.update__set_to_dummy();
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

  ValueT pop(const KeyT &key)
  {
    BLI_assert(this->contains(key));
    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.has_key(offset, key)) {
        ValueT value = std::move(*item.value(offset));
        item.set_dummy(offset);
        m_array.update__set_to_dummy();
        return value;
      }
    }
    ITER_SLOTS_END(offset);
  }

  bool contains(const KeyT &key) const
  {
    ITER_SLOTS_BEGIN (key, m_array, const, item, offset) {
      if (item.status(offset) == IS_EMPTY) {
        return false;
      }
      else if (item.has_key(offset, key)) {
        return true;
      }
    }
    ITER_SLOTS_END(offset);
  }

  template<typename CreateValueF, typename ModifyValueF>
  bool add_or_modify(const KeyT &key,
                     const CreateValueF &create_value,
                     const ModifyValueF &modify_value)
  {
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.copy_in(offset, key, create_value());
        m_array.update__empty_to_set();
        return true;
      }
      else if (item.has_key(offset, key)) {
        modify_value(*item.value(offset));
        return false;
      }
    }
    ITER_SLOTS_END(offset);
  }

  bool add_override(const KeyT &key, const ValueT &value)
  {
    return this->add_or_modify(
        key, [&value]() { return value; }, [&value](ValueT &old_value) { old_value = value; });
  }

  ValueT *lookup_ptr(const KeyT &key) const
  {
    ITER_SLOTS_BEGIN (key, m_array, const, item, offset) {
      if (item.is_empty(offset)) {
        return nullptr;
      }
      else if (item.has_key(offset, key)) {
        return item.value(offset);
      }
    }
    ITER_SLOTS_END(offset);
  }

  ValueT &lookup(const KeyT &key) const
  {
    return *this->lookup_ptr(key);
  }

  ValueT lookup_default(const KeyT &key, ValueT default_value) const
  {
    ValueT *ptr = this->lookup_ptr(key);
    if (ptr != nullptr) {
      return *ptr;
    }
    else {
      return default_value;
    }
  }

  template<typename CreateValueF>
  ValueT &lookup_or_add(const KeyT &key, const CreateValueF &create_value)
  {
    this->ensure_can_add();

    ITER_SLOTS_BEGIN (key, m_array, , item, offset) {
      if (item.is_empty(offset)) {
        item.copy_in(offset, key, create_value());
        m_array.update__empty_to_set();
        return *item.value(offset);
      }
      else if (item.has_key(offset, key)) {
        return *item.value(offset);
      }
    }
    ITER_SLOTS_END(offset);
  }

  uint32_t size() const
  {
    return m_array.slots_set();
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
        uint8_t status = item.status(offset);
        if (status == IS_EMPTY) {
          std::cout << "    <empty>\n";
        }
        else if (status == IS_SET) {
          const KeyT &key = *item.key(offset);
          const ValueT &value = *item.value(offset);
          uint32_t collisions = this->count_collisions(value);
          std::cout << "    " << key << " -> " << value << "  \t Collisions: " << collisions
                    << '\n';
        }
        else if (status == IS_DUMMY) {
          std::cout << "    <dummy>\n";
        }
      }
    }
  }

  template<typename SubIterator> class BaseIterator {
   protected:
    const Map *m_map;
    uint32_t m_slot;

   public:
    BaseIterator(const Map *map, uint32_t slot) : m_map(map), m_slot(slot)
    {
    }

    BaseIterator &operator++()
    {
      m_slot = m_map->next_slot(m_slot + 1);
      return *this;
    }

    friend bool operator==(const BaseIterator &a, const BaseIterator &b)
    {
      BLI_assert(a.m_map == b.m_map);
      return a.m_slot == b.m_slot;
    }

    friend bool operator!=(const BaseIterator &a, const BaseIterator &b)
    {
      return !(a == b);
    }

    SubIterator begin() const
    {
      return SubIterator(m_map, m_map->next_slot(0));
    }

    SubIterator end() const
    {
      return SubIterator(m_map, m_map->m_array.slots_total());
    }
  };

  class KeyIterator final : public BaseIterator<KeyIterator> {
   public:
    KeyIterator(const Map *map, uint32_t slot) : BaseIterator<KeyIterator>(map, slot)
    {
    }

    const KeyT &operator*() const
    {
      uint32_t item_index = this->m_slot >> 2;
      uint32_t offset = this->m_slot & OFFSET_MASK;
      const Item &item = this->m_map->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return *item.key(offset);
    }
  };

  class ValueIterator final : public BaseIterator<ValueIterator> {
   public:
    ValueIterator(const Map *map, uint32_t slot) : BaseIterator<ValueIterator>(map, slot)
    {
    }

    ValueT &operator*() const
    {
      uint32_t item_index = this->m_slot >> 2;
      uint32_t offset = this->m_slot & OFFSET_MASK;
      const Item &item = this->m_map->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return *item.value(offset);
    }
  };

  class ItemIterator final : public BaseIterator<ItemIterator> {
   public:
    ItemIterator(const Map *map, uint32_t slot) : BaseIterator<ItemIterator>(map, slot)
    {
    }

    struct UserItem {
      const KeyT &key;
      ValueT &value;

      friend std::ostream &operator<<(std::ostream &stream, const Item &item)
      {
        stream << item.key << " -> " << item.value;
        return stream;
      }
    };

    UserItem operator*() const
    {
      uint32_t item_index = this->m_slot >> 2;
      uint32_t offset = this->m_slot & OFFSET_MASK;
      const Item &item = this->m_map->m_array.item(item_index);
      BLI_assert(item.is_set(offset));
      return {*item.key(offset), *item.value(offset)};
    }
  };

  template<typename SubIterator> friend class BaseIterator;

  KeyIterator keys() const
  {
    return KeyIterator(this, 0);
  }

  ValueIterator values() const
  {
    return ValueIterator(this, 0);
  }

  ItemIterator items() const
  {
    return ItemIterator(this, 0);
  }

 private:
  uint32_t next_slot(uint32_t slot) const
  {
    for (; slot < m_array.slots_total(); slot++) {
      uint32_t item_index = slot >> 2;
      uint32_t offset = slot & OFFSET_MASK;
      const Item &item = m_array.item(item_index);
      if (item.is_set(offset)) {
        return slot;
      }
    }
    return slot;
  }

  uint32_t count_collisions(const KeyT &key) const
  {
    uint32_t collisions = 0;
    ITER_SLOTS_BEGIN (key, m_array, const, item, offset) {
      if (item.status(offset) == IS_EMPTY || item.has_key(offset, key)) {
        return collisions;
      }
      collisions++;
    }
    ITER_SLOTS_END(offset);
  }

  void ensure_can_add()
  {
    if (m_array.should_grow()) {
      this->grow(this->size() + 1);
    }
  }

  void grow(uint32_t min_usable_slots)
  {
    ArrayType new_array = m_array.init_reserved(min_usable_slots);
    for (Item &old_item : m_array) {
      for (uint32_t offset = 0; offset < 4; offset++) {
        if (old_item.status(offset) == IS_SET) {
          this->add_after_grow(*old_item.key(offset), *old_item.value(offset), new_array);
        }
      }
    }
    m_array = std::move(new_array);
  }

  void add_after_grow(KeyT &key, ValueT &value, ArrayType &new_array)
  {
    ITER_SLOTS_BEGIN (key, new_array, , item, offset) {
      if (item.status(offset) == IS_EMPTY) {
        item.move_in(offset, key, value);
        return;
      }
    }
    ITER_SLOTS_END(offset);
  }

#undef ITER_SLOTS_BEGIN
#undef ITER_SLOTS_END
};

};  // namespace BLI
