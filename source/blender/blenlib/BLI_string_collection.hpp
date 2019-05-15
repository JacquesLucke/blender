#pragma once

/* Utility classes to store multiple strings in a continuous
 * memory buffer. */

#include "BLI_string_ref.hpp"
#include "BLI_small_vector.hpp"

namespace BLI {

class StringCollection {
 private:
  const char *m_data;
  SmallVector<StringRefNull> m_strings;

  StringCollection(StringCollection &other) = delete;

 public:
  StringCollection(const char *data, SmallVector<StringRefNull> strings)
      : m_data(data), m_strings(std::move(strings))
  {
  }

  ~StringCollection()
  {
    MEM_freeN((void *)m_data);
  }

  StringRefNull get_ref(uint index) const
  {
    return m_strings[index];
  }

  uint size() const
  {
    return m_strings.size();
  }
};

class StringCollectionBuilder {
 private:
  SmallVector<char, 64> m_chars;
  SmallVector<uint> m_offsets;

 public:
  StringCollectionBuilder() = default;

  /* Returns the index in the final collection */
  uint insert(StringRef ref)
  {
    m_offsets.append(m_chars.size());
    m_chars.extend(ref.data(), ref.size());
    m_chars.append('\0');
    return m_offsets.size() - 1;
  }

  StringCollection *build()
  {
    char *data = (char *)MEM_mallocN(m_chars.size(), __func__);
    memcpy(data, m_chars.begin(), m_chars.size());
    SmallVector<StringRefNull> references;

    for (uint start_offset : m_offsets) {
      references.append(StringRefNull(data + start_offset));
    }

    return new StringCollection(data, references);
  }
};

}  // namespace BLI
