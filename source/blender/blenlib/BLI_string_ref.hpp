#pragma once

/* Two types of string references. One that guarantees null termination
 * and one that does not. */

#include <cstring>
#include <string>

#include "BLI_utildefines.h"

namespace BLI {

class StringRefBase {
 public:
  using size_type = size_t;

 protected:
  const char *m_data;
  size_type m_size;

  StringRefBase(const char *data, size_type size) : m_data(data), m_size(size)
  {
  }

 public:
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

  std::string to_std_string() const
  {
    return std::string(m_data, m_size);
  }

  friend std::ostream &operator<<(std::ostream &stream, StringRefBase ref)
  {
    stream << ref.to_std_string();
    return stream;
  }

  friend std::string operator+(const StringRefBase a, const StringRefBase b)
  {
    return a.to_std_string() + b.data();
  }

  friend std::string operator+(const char *a, const StringRefBase b)
  {
    return a + b.to_std_string();
  }

  friend std::string operator+(const StringRefBase a, const char *b)
  {
    return a.to_std_string() + b;
  }

  friend std::string operator+(const std::string &a, const StringRefBase b)
  {
    return a + b.data();
  }

  friend std::string operator+(const StringRefBase a, const std::string &b)
  {
    return a.data() + b;
  }
};

class StringRefNull : public StringRefBase {

 public:
  StringRefNull() : StringRefBase("", 0)
  {
  }

  StringRefNull(const char *str) : StringRefBase(str, strlen(str))
  {
    BLI_assert(str != NULL);
    BLI_assert(m_data[m_size] == '\0');
  }

  StringRefNull(const std::string &str) : StringRefNull(str.data())
  {
  }
};

class StringRef : public StringRefBase {
 public:
  StringRef() : StringRefBase(nullptr, 0)
  {
  }

  StringRef(StringRefNull other) : StringRefBase(other.data(), other.size())
  {
  }

  StringRef(const char *str) : StringRefBase(str, str ? strlen(str) : 0)
  {
  }

  StringRef(const char *str, size_type length) : StringRefBase(str, length)
  {
  }

  StringRef(const std::string &str) : StringRefBase(str.data(), str.size())
  {
  }
};

}  // namespace BLI
