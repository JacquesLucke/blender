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
 * Two types of string references. One that guarantees null termination
 * and one that does not. */

#pragma once

#include <cstring>
#include <string>

#include "BLI_utildefines.h"

namespace BLI {

class StringRef;

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
  /**
   * Return the (byte-)length of the referenced string, without any null-terminator.
   */
  size_type size() const
  {
    return m_size;
  }

  /**
   * Return a pointer to the start of the string.
   */
  const char *data() const
  {
    return m_data;
  }

  char operator[](size_type index) const
  {
    BLI_assert(index <= m_size);
    return m_data[index];
  }

  const char *begin() const
  {
    return m_data;
  }

  const char *end()
  {
    return m_data + m_size;
  }

  bool startswith(StringRef prefix) const;

  bool endswith(StringRef suffix) const;

  /**
   * Convert the referenced string into a std::string object.
   */
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

  friend bool operator==(const StringRefBase a, const StringRefBase b)
  {
    if (a.size() != b.size()) {
      return false;
    }
    return STREQLEN(a.data(), b.data(), a.size());
  }

  friend bool operator==(const StringRefBase a, const char *b)
  {
    return STREQ(a.data(), b);
  }

  friend bool operator==(const char *a, const StringRefBase b)
  {
    return b == a;
  }

  friend bool operator==(const StringRefBase a, const std::string &b)
  {
    return a == StringRefBase(b.data(), b.size());
  }

  friend bool operator==(const std::string &a, const StringRefBase b)
  {
    return b == a;
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

  StringRefNull(const char *str, size_type size) : StringRefBase(str, size)
  {
    BLI_assert(str[size] == '\0');
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

/* More inline functions
 ***************************************/

inline bool StringRefBase::startswith(StringRef prefix) const
{
  if (m_size < prefix.m_size) {
    return false;
  }
  for (uint i = 0; i < prefix.m_size; i++) {
    if (m_data[i] != prefix.m_data[i]) {
      return false;
    }
  }
  return true;
}

inline bool StringRefBase::endswith(StringRef suffix) const
{
  if (m_size < suffix.m_size) {
    return false;
  }
  uint offset = m_size - suffix.m_size;
  for (uint i = 0; i < suffix.m_size; i++) {
    if (m_data[offset + i] != suffix.m_data[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace BLI
