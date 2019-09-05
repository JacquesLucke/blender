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
 * Utility class that represents a range that has been split up into chunks.
 */

#pragma once

#include "BLI_index_range.hpp"

namespace BLI {

class ChunkedIndexRange {
 private:
  IndexRange m_total_range;
  uint m_chunk_size;
  uint m_chunk_amount;

 public:
  ChunkedIndexRange(IndexRange total_range, uint chunk_size)
      : m_total_range(total_range),
        m_chunk_size(chunk_size),
        m_chunk_amount(std::ceil(m_total_range.size() / (float)m_chunk_size))
  {
  }

  uint chunks() const
  {
    return m_chunk_amount;
  }

  IndexRange chunk_range(uint index) const
  {
    BLI_assert(index < m_chunk_amount);
    uint start = m_total_range[index * m_chunk_size];
    uint size = std::min<uint>(m_chunk_size, m_total_range.one_after_last() - start);
    return IndexRange(start, size);
  }
};

}  // namespace BLI
