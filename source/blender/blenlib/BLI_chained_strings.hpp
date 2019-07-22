#pragma once

/* These classes help storing multiple strings more compactly.
 * This should only be used when:
 *   - All strings are freed at the same time.
 *   - The length of individual strings does not change.
 *   - All string lengths are known in the beginning. */

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
