#pragma once

#include "BLI_optional.hpp"

namespace BLI {

struct ErrorInfo {
  const char *file;
  uint line;
  const char *function;
  std::string message;
};

/**
 * This class can be used as return value of functions that might have an error.
 * The main reason it exists, is that I'm not sure whether we should use exceptions or not. That
 * needs to be discussed at some point.
 */
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
    ErrorInfo \
    { \
      __FILE__, __LINE__, __func__, MESSAGE \
    } \
  }
