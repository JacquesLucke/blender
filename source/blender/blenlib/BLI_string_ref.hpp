#pragma once

/* Two types of string references. One that guarantees null termination
 * and one that does not. */

#include <cstring>
#include <string>

#include "BLI_utildefines.h"

namespace BLI {

class StringRefNull {
 public:
  using size_type = size_t;

 private:
  const char *m_data;
  size_type m_size;

 public:
  StringRefNull() : m_data(""), m_size(0)
  {
  }

  StringRefNull(const char *str) : m_data(str), m_size(strlen(str))
  {
    BLI_assert(str != NULL);
    BLI_assert(m_data[m_size] == '\0');
  }

  StringRefNull(const std::string &str) : StringRefNull(str.data())
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
    BLI_assert(index <= m_size);
    return m_data[index];
  }
};

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

  StringRef(StringRefNull other) : m_data(other.data()), m_size(other.size())
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
