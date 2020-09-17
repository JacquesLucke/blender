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

#include "BLI_stack.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename Allocator = GuardedAllocator> class MemoryPool : NonCopyable, NonMovable {
 private:
  int64_t element_size_;
  int64_t element_alignment_;
  int64_t next_allocation_size_ = 4;
  int64_t total_allocated_elements_ = 0;
  const char *debug_name_;
  Allocator allocator_;
  Vector<void *, 4, Allocator> buffers_;
  Stack<void *, 4, Allocator> free_list_;

 public:
  MemoryPool(const int64_t element_size,
             const int64_t element_alignment,
             const char *debug_name = AT)
      : element_size_(element_size), element_alignment_(element_alignment), debug_name_(debug_name)
  {
  }

  ~MemoryPool()
  {
    BLI_assert(free_list_.size() == total_allocated_elements_);
    for (void *buffer : buffers_) {
      allocator_.deallocate(buffer);
    }
  }

  void *allocate()
  {
    if (!free_list_.is_empty()) {
      return free_list_.pop();
    }

    const int64_t new_element_amount = next_allocation_size_;
    void *new_buffer = allocator_.allocate(
        element_size_ * new_element_amount, element_alignment_, debug_name_);
    buffers_.append(new_buffer);
    total_allocated_elements_ += new_element_amount;

    /* Push elements in reverse order, so that they will be allocated in order of increasing memory
     * addresses. */
    char *current_element = (char *)new_buffer + (new_element_amount - 1) * element_size_;
    for (int64_t i = 0; i < new_element_amount; i++) {
      free_list_.push(current_element);
      current_element -= element_size_;
    }

    /* Next time allocate more elements at once. */
    next_allocation_size_ = (next_allocation_size_ * 3) / 2;

    return free_list_.pop();
  }

  void deallocate(void *address)
  {
    free_list_.push(address);
  }
};

template<typename T, typename Allocator = GuardedAllocator> class TypedMemoryPool {
 private:
  MemoryPool<Allocator> memory_pool_;

 public:
  TypedMemoryPool(const char *debug_name = "") : memory_pool_(sizeof(T), alignof(T), debug_name)
  {
  }

  void *allocate()
  {
    return memory_pool_.allocate();
  }

  void deallocate(void *value)
  {
    memory_pool_.deallocate(value);
  }

  template<typename... Args> T *allocate_and_construct(Args &&... args)
  {
    void *ptr = memory_pool_.allocate();
    T *value = new (ptr) T(std::forward<Args>(args)...);
    return value;
  }

  void destruct_and_deallocate(T *value)
  {
    value->~T();
    memory_pool_.deallocate(value);
  }
};

}  // namespace blender
