#pragma once

#include "BLI_utildefines.h"
#include "BLI_math_base.h"

namespace BLI {

class BitReference {
 private:
  uint8_t *m_ptr;
  uint8_t m_bit_mask;

 public:
  BitReference(uint8_t *ptr, uint8_t bit_mask) : m_ptr(ptr), m_bit_mask(bit_mask)
  {
    BLI_assert(is_power_of_2_i(bit_mask));
  }

  bool is_set() const
  {
    return *m_ptr & m_bit_mask;
  }

  void set()
  {
    *m_ptr |= m_bit_mask;
  }

  void unset()
  {
    *m_ptr &= ~m_bit_mask;
  }
};

class MutableBitArrayRef {
 private:
  uint8_t *m_data = nullptr;
  uint m_offset = 0;
  uint m_size = 0;

 public:
  MutableBitArrayRef() = default;

  MutableBitArrayRef(uint8_t *data, uint size) : m_data(data), m_offset(0), m_size(size)
  {
  }

  MutableBitArrayRef(uint8_t *data, uint offset, uint size)
      : m_data(data), m_offset(offset), m_size(size)
  {
  }

  explicit MutableBitArrayRef(uint8_t &data)
      : m_data(&data), m_offset(0), m_size(sizeof(uint8_t) * 8)
  {
  }

  MutableBitArrayRef slice(uint start, uint size) const
  {
    BLI_assert(start + size <= m_size || size == 0);
    return MutableBitArrayRef(m_data, m_offset + start, size);
  }

  uint size() const
  {
    return m_size;
  }

  BitReference operator[](uint index)
  {
    BLI_assert(index < m_size);
    uint bit_index = m_offset + index;
    uint8_t *ptr = m_data + bit_index / 8;
    uint8_t offset = bit_index % 8;
    uint8_t bit_mask = 1 << offset;
    return BitReference(ptr, bit_mask);
  }

  const BitReference operator[](uint index) const
  {
    return (*const_cast<MutableBitArrayRef *>(this))[index];
  }

  bool is_set(uint index) const
  {
    return (*this)[index].is_set();
  }

  void set(uint index)
  {
    return (*this)[index].set();
  }

  void unset(uint index)
  {
    return (*this)[index].unset();
  }
};

};  // namespace BLI
