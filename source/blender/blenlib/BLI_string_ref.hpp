#pragma once

/* Reference to a string that might NOT be null-terminated. */

#include <cstring>
#include <string>

#include "BLI_utildefines.h"

namespace BLI {

class StringRef {
 public:
  using size_type = size_t;

 private:
  const char *m_data;
  size_type m_size;

 public:
  StringRef() : m_data(nullptr), m_size(0)
  {
  }

  StringRef(const char *str) : m_data(str), m_size(str ? strlen(str) : 0)
  {
  }

  StringRef(const char *str, size_type length) : m_data(str), m_size(length)
  {
  }

  StringRef(const std::string &str) : m_data(str.data()), m_size(str.size())
  {
  }

  size_type size() const
  {
    return m_size;
  }

  const char *data() const
  {
    return m_data;
  }

  char operator[](size_type index) const
  {
    BLI_assert(index < m_size);
    return m_data[index];
  }

  std::string to_std_string() const
  {
    return std::string(m_data, m_size);
  }

  friend std::ostream &operator<<(std::ostream &stream, StringRef ref)
  {
    stream << ref.to_std_string();
    return stream;
  }
};

}  // namespace BLI
