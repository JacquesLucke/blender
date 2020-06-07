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

#ifndef __BLI_QUEUE_HH__
#define __BLI_QUEUE_HH__

/** \file
 * \ingroup bli
 */

#include "BLI_allocator.hh"
#include "BLI_array_ref.hh"
#include "BLI_memory_utils.hh"

namespace BLI {

template<typename T, uint InlineBufferCapacity = 4, typename Allocator = GuardedAllocator>
class Queue {
 private:
  T *m_data;
  uint64_t m_enqueue_pos;
  uint64_t m_dequeue_pos;
  uint64_t m_index_mask;
  Allocator m_allocator;

  AlignedBuffer<sizeof(T) * InlineBufferCapacity, alignof(T)> m_inline_buffer;

 public:
  Queue()
  {
    BLI_assert(is_power_of_2_i((int)InlineBufferCapacity));
    m_data = this->inline_buffer();
    m_enqueue_pos = 0;
    m_dequeue_pos = 0;
    m_index_mask = InlineBufferCapacity - 1;
  }

  ~Queue()
  {
    MutableArrayRef<T> elements1, elements2;
    this->get_elements(elements1, elements2);
    destruct_n(elements1.data(), elements1.size());
    destruct_n(elements2.data(), elements2.size());
    if (m_data != this->inline_buffer()) {
      m_allocator.deallocate(m_data);
    }
  }

  void enqueue(const T &value)
  {
    this->ensure_space_for_one();
    uint index = m_enqueue_pos & m_index_mask;
    new (m_data + index) T(value);
    m_enqueue_pos++;
  }

  T dequeue()
  {
    BLI_assert(m_enqueue_pos != m_dequeue_pos);
    uint32_t index = m_dequeue_pos & m_index_mask;
    T value = std::move(m_data[index]);
    m_data[index].~T();
    m_dequeue_pos++;
    return value;
  }

  uint32_t size() const
  {
    return (uint32_t)(m_enqueue_pos - m_dequeue_pos);
  }

  uint32_t capacity() const
  {
    return m_index_mask + 1;
  }

  bool is_empty() const
  {
    return m_enqueue_pos == m_dequeue_pos;
  }

 private:
  void ensure_space_for_one()
  {
    uint32_t size = this->size();
    if (size > m_index_mask) {
      this->realloc_to_at_least(size + 1);
    }
  }

  BLI_NOINLINE void realloc_to_at_least(uint32_t min_capacity)
  {
    if (this->capacity() >= min_capacity) {
      return;
    }

    uint32_t new_capacity = power_of_2_max_u(min_capacity);

    T *new_data = (T *)m_allocator.allocate(new_capacity * sizeof(T), alignof(T), AT);

    MutableArrayRef<T> elements1, elements2;
    this->get_elements(elements1, elements2);
    uninitialized_relocate_n(elements1.data(), elements1.size(), new_data);
    uninitialized_relocate_n(elements2.data(), elements2.size(), new_data + elements1.size());

    if (m_data != this->inline_buffer()) {
      m_allocator.deallocate(m_data);
    }

    m_dequeue_pos = 0;
    m_enqueue_pos = elements1.size() + elements2.size();
    m_data = new_data;
    m_index_mask = new_capacity - 1;
  }

  void get_elements(MutableArrayRef<T> &r_elements_1, MutableArrayRef<T> &r_elements_2)
  {
    uint64_t capacity = this->capacity();
    uint64_t wrap_after_dequeue_pos = (m_dequeue_pos + capacity + 1) & ~m_index_mask;

    if (wrap_after_dequeue_pos < m_enqueue_pos) {
      r_elements_1 = MutableArrayRef<T>(m_data + (m_dequeue_pos & m_index_mask),
                                        wrap_after_dequeue_pos - m_dequeue_pos);
      r_elements_2 = MutableArrayRef<T>(m_data + (wrap_after_dequeue_pos & m_index_mask),
                                        m_enqueue_pos - wrap_after_dequeue_pos);
    }
    else {
      r_elements_1 = MutableArrayRef<T>(m_data + (m_dequeue_pos & m_index_mask),
                                        m_enqueue_pos - m_dequeue_pos);
      r_elements_2 = {};
    }
  }

  T *inline_buffer()
  {
    return (T *)m_inline_buffer.ptr();
  }
};

}  // namespace BLI

#endif /* __BLI_QUEUE_HH__ */
