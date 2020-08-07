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

#pragma once

#include "BLI_linear_allocator.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

namespace blender {

class TrieNodeSmallHead;
class TrieNodeLargeHead;

static int trie_node_type_to_hash_table_size(const int node_type)
{
  if (node_type == 0) {
    return 0;
  }
  return 1 << node_type;
}

template<typename Head>
static uint8_t get_hash_table_pointers(const Head &head,
                                       const void *start,
                                       const uint8_t **r_bytes,
                                       const TrieNodeSmallHead ***r_children)
{
  const int type = head.node_type();
  const int table_size = trie_node_type_to_hash_table_size(type);
  const uint8_t hash_mask = table_size == 0 ? 0u : static_cast<uint8_t>(table_size) - 1u;
  *r_bytes = static_cast<const uint8_t *>(start);
  *r_children = static_cast<const TrieNodeSmallHead **>(r_bytes + table_size);
  return hash_mask;
}

class TrieNodeSmallHead {
 private:
  uint64_t data_;

  static constexpr inline uint64_t IS_TERMINAL_MASK = 0b00000001;
  static constexpr inline uint64_t IS_POINTER_MASK = 0b00000010;
  static constexpr inline uint64_t STRING_LENGTH_MASK = 0b00011100;
  static constexpr inline uint64_t NODE_TYPE_MASK = 0b11100000;
  static constexpr inline int STRING_LENGTH_SHIFT = 2;
  static constexpr inline int NODE_TYPE_SHIFT = 5;

 public:
  bool is_actually_large_head() const
  {
    return this->is_pointer();
  }

  bool is_terminal() const
  {
    return (data_ & IS_TERMINAL_MASK) != 0;
  }

  bool set_terminal()
  {
    const bool was_terminal = this->is_terminal();
    data_ |= IS_TERMINAL_MASK;
    const bool terminal_status_changed = !was_terminal;
    return terminal_status_changed;
  }

  bool is_pointer() const
  {
    return (data_ & IS_POINTER_MASK) != 0;
  }

  int64_t string_length() const
  {
    BLI_assert(!this->is_pointer());
    return static_cast<int64_t>((data_ & STRING_LENGTH_MASK) >> STRING_LENGTH_SHIFT);
  }

  int node_type() const
  {
    BLI_assert(!this->is_pointer());
    return static_cast<int>((data_ & NODE_TYPE_MASK) >> NODE_TYPE_SHIFT);
  }

  TrieNodeLargeHead &as_large_head()
  {
    BLI_assert(this->is_pointer());
    return reinterpret_cast<TrieNodeLargeHead &>(*this);
  }

  const TrieNodeLargeHead &as_large_head() const
  {
    BLI_assert(this->is_pointer());
    return reinterpret_cast<const TrieNodeLargeHead &>(*this);
  }

  uint8_t hash_table(uint8_t **r_bytes, TrieNodeSmallHead ***r_children)
  {
    return const_cast<const TrieNodeSmallHead *>(this)->hash_table(
        const_cast<const uint8_t **>(r_bytes),
        const_cast<const TrieNodeSmallHead ***>(r_children));
  }

  uint8_t hash_table(const uint8_t **r_bytes, const TrieNodeSmallHead ***r_children) const
  {
    return get_hash_table_pointers(
        *this, POINTER_OFFSET(this, sizeof(uint64_t)), r_bytes, r_children);
  }
};

class TrieNodeLargeHead {
 private:
  TrieNodeSmallHead small_head_;
  uint32_t data_;

  static constexpr inline uint32_t NODE_TYPE_MASK = 0b111;
  static constexpr inline uint32_t STRING_LENGTH_MASK = ~NODE_TYPE_MASK;
  static constexpr inline int STRING_LENGTH_SHIFT = 3;

 public:
  int64_t string_length() const
  {
    return static_cast<int64_t>((data_ & STRING_LENGTH_MASK) >> STRING_LENGTH_SHIFT);
  }

  int node_type() const
  {
    return static_cast<int>(data_ & NODE_TYPE_MASK);
  }

  uint8_t hash_table(uint8_t **r_bytes, TrieNodeSmallHead ***r_children)
  {
    return const_cast<const TrieNodeLargeHead *>(this)->hash_table(
        const_cast<const uint8_t **>(r_bytes),
        const_cast<const TrieNodeSmallHead ***>(r_children));
  }

  uint8_t hash_table(const uint8_t **r_bytes, const TrieNodeSmallHead ***r_children) const
  {
    return get_hash_table_pointers(
        *this, POINTER_OFFSET(this, sizeof(uint64_t) + sizeof(uint32_t)), r_bytes, r_children);
  }
};

class Trie {
 private:
  LinearAllocator<> allocator_;
  TrieNodeSmallHead *root_;

 public:
  Trie()
  {
    root_ = &this->allocate_small(0);
  }

  bool add(StringRef str)
  {
    return this->add(Span<uint8_t>((const uint8_t *)str.data(), str.size()));
  }
  bool add(Span<uint8_t> values)
  {
    Span<uint8_t> remaining_values = values;
    TrieNodeSmallHead *current = root_;
    TrieNodeSmallHead **pointer_to_current = &root_;

    while (true) {
      if (remaining_values.is_empty()) {
        return current->set_terminal();
      }

      const uint8_t first_value = remaining_values[0];
      remaining_values = remaining_values.drop_front(1);

      uint8_t hash_mask;
      uint8_t *hash_table_bytes;
      TrieNodeSmallHead **hash_table_pointers;

      if (current->is_actually_large_head()) {
        TrieNodeLargeHead &current_large = current->as_large_head();
        hash_mask = current_large.hash_table(&hash_table_bytes, &hash_table_pointers);
      }
      else {
        TrieNodeSmallHead &current_small = *current;
        hash_mask = current_small.hash_table(&hash_table_bytes, &hash_table_pointers);
      }

      const uint32_t iterations = hash_mask == 0 ? 0 : hash_mask + 1;

      bool did_find_slot = false;
      uint8_t found_slot = 0;
      for (uint32_t offset = 0; offset < iterations; offset++) {
        const uint8_t slot = (first_value + offset) & hash_mask;
        if (hash_table_bytes[slot] == first_value) {
          did_find_slot = true;
          found_slot = slot;
          break;
        }
        if (hash_table_pointers[slot] == nullptr) {
          did_find_slot = true;
          found_slot = slot;
          break;
        }
      }

      if (!did_find_slot) {
        /* TODO: Grow current node. */
      }

      TrieNodeSmallHead *child_head = hash_table_pointers[found_slot];
      if (child_head == nullptr) {
        /* TODO: Allocate new child node. */
      }
    }
  }

 private:
  TrieNodeSmallHead &allocate_small(int node_type)
  {
    int table_size = trie_node_type_to_hash_table_size(node_type);
    int64_t size = 8 + 9 * table_size;
    return *static_cast<TrieNodeSmallHead *>(allocator_.allocate(size, 1));
  }

  TrieNodeLargeHead &allocate_large(int node_type)
  {
    int table_size = trie_node_type_to_hash_table_size(node_type);
    int64_t size = 12 + 9 * table_size;
    return *static_cast<TrieNodeLargeHead *>(allocator_.allocate(size, 1));
  }
};

}  // namespace blender
