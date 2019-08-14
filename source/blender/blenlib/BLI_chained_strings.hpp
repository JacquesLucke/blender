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
 * These classes help storing multiple strings more compactly.
 * This should only be used when:
 *   - All strings are freed at the same time.
 *   - The length of individual strings does not change.
 *   - All string lengths are known in the beginning. */

#pragma once

#include "BLI_string_ref.hpp"
#include "BLI_vector.hpp"

namespace BLI {

class ChainedStringRef {
  uint32_t m_start : 24;
  uint32_t m_size : 8;

 public:
  ChainedStringRef(uint start, uint size) : m_start(start), m_size(size)
  {
    BLI_assert(size < (1 << 8));
    BLI_assert(start < (1 << 24));
  }

  uint size() const
  {
    return m_size;
  }

  StringRefNull to_string_ref(const char *ptr) const
  {
    return StringRefNull(ptr + m_start, m_size);
  }
};

class ChainedStringsBuilder {
 public:
  ChainedStringRef add(StringRef str)
  {
    ChainedStringRef ref(m_chars.size(), str.size());
    m_chars.extend(str.data(), str.size());
    m_chars.append(0);
    return ref;
  }

  char *build()
  {
    char *ptr = (char *)MEM_mallocN(m_chars.size(), __func__);
    memcpy(ptr, m_chars.begin(), m_chars.size());
    return ptr;
  }

 private:
  Vector<char, 64> m_chars;
};

};  // namespace BLI
