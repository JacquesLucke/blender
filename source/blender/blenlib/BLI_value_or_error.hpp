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
 * This class can be used as return value of functions that might have an error. This forces the
 * caller to check if there was an error or not.
 *
 * The benefit over just using BLI::optional is that this can also store more information about the
 * error.
 */

#pragma once

#include <string>
#include "BLI_optional.hpp"

namespace BLI {

struct ErrorInfo {
  const char *file;
  uint line;
  const char *function;
  std::string message;
};

template<typename T> class ValueOrError {
 private:
  Optional<T> m_value;
  ErrorInfo m_error;

 public:
  ValueOrError(T value) : m_value(std::move(value))
  {
  }

  ValueOrError(ErrorInfo error) : m_error(std::move(error))
  {
  }

  static ValueOrError FromError(const char *file,
                                uint line,
                                const char *function,
                                std::string message)
  {
    return ValueOrError(ErrorInfo{file, line, function, message});
  }

  bool is_error() const
  {
    return !m_value.has_value();
  }

  T extract_value()
  {
    BLI_assert(m_value.has_value());
    return m_value.extract();
  }

  ErrorInfo &error()
  {
    return m_error;
  }
};

}  // namespace BLI

#define BLI_ERROR_CREATE(MESSAGE) \
  { \
    BLI::ErrorInfo \
    { \
      __FILE__, __LINE__, __func__, MESSAGE \
    } \
  }
