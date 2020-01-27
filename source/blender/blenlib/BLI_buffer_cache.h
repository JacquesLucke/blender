#ifndef __BLI_BUFFER_ALLOCATOR_H__
#define __BLI_BUFFER_ALLOCATOR_H__

#include "BLI_vector.h"

namespace BLI {

class BufferCache {
 private:
  static const int Alignment = 64;

  struct BufferHead {
    uint buffer_size_in_bytes;

    void *user_ptr()
    {
      BLI_STATIC_ASSERT(sizeof(BufferHead) <= Alignment, "");
      return POINTER_OFFSET(this, Alignment);
    }

    static BufferHead *FromUserPtr(void *ptr)
    {
      return (BufferHead *)POINTER_OFFSET(ptr, -Alignment);
    }
  };

  Vector<BufferHead *, 16> m_all_buffers;
  Vector<BufferHead *, 16> m_cached_buffers;

 public:
  BufferCache() = default;

  ~BufferCache()
  {
    BLI_assert(m_cached_buffers.size() == m_all_buffers.size());

    for (BufferHead *head : m_all_buffers) {
      MEM_freeN((void *)head);
    }
  }

  void *allocate(uint size, uint alignment)
  {
    UNUSED_VARS_NDEBUG(alignment);
    BLI_assert(alignment <= Alignment);

    /* Only use buffer sizes that are a power of two, to make them easier to reuse. */
    uint padded_size = power_of_2_max_u(size);

    /* Try to use a cached memory buffer. Start searching from the back to prefer buffers that have
     * been used "just before". */
    for (int i = m_cached_buffers.size() - 1; i >= 0; i--) {
      BufferHead *head = m_cached_buffers[i];
      if (head->buffer_size_in_bytes == padded_size) {
        void *user_ptr = head->user_ptr();
        m_cached_buffers.remove_and_reorder(i);
        // std::cout << "Reuse buffer\n";
        return user_ptr;
      }
    }

    BufferHead *new_head = (BufferHead *)MEM_mallocN_aligned(
        padded_size + Alignment, Alignment, "allocate in BufferCache");
    new_head->buffer_size_in_bytes = padded_size;
    m_all_buffers.append(new_head);
    // std::cout << "New buffer\n";
    return new_head->user_ptr();
  }

  void deallocate(void *buffer)
  {
    BufferHead *head = BufferHead::FromUserPtr(buffer);
    BLI_assert(m_all_buffers.contains(head));
    m_cached_buffers.append(head);
  }

  void *allocate(uint element_amount, uint element_size, uint alignment)
  {
    return this->allocate(element_amount * element_size, alignment);
  }
};

}  // namespace BLI

#endif /* __BLI_BUFFER_ALLOCATOR_H__ */
