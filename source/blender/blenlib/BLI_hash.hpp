#pragma once

#include <functional>
#include <string>
#include <utility>

#include "BLI_utildefines.h"
#include "BLI_math_base.h"

namespace BLI {

template<typename T> struct DefaultHash {
};

#define TRIVIAL_DEFAULT_INT_HASH(TYPE) \
  template<> struct DefaultHash<TYPE> { \
    uint32_t operator()(TYPE value) const \
    { \
      return (uint32_t)value; \
    } \
  }

TRIVIAL_DEFAULT_INT_HASH(int8_t);
TRIVIAL_DEFAULT_INT_HASH(uint8_t);
TRIVIAL_DEFAULT_INT_HASH(int16_t);
TRIVIAL_DEFAULT_INT_HASH(uint16_t);
TRIVIAL_DEFAULT_INT_HASH(int32_t);
TRIVIAL_DEFAULT_INT_HASH(uint32_t);
TRIVIAL_DEFAULT_INT_HASH(int64_t);

template<> struct DefaultHash<std::string> {
  uint32_t operator()(const std::string &value) const
  {
    uint32_t hash = 5381;
    for (char c : value) {
      hash = hash * 33 + c;
    }
    return hash;
  }
};

template<typename T> struct DefaultHash<T *> {
  uint32_t operator()(const T *value) const
  {
    uintptr_t ptr = POINTER_AS_UINT(value);
    uint32_t hash = (uint32_t)(ptr >> 3);
    return hash;
  }
};

template<typename T1, typename T2> struct DefaultHash<std::pair<T1, T2>> {
  uint32_t operator()(const std::pair<T1, T2> &value) const
  {
    uint32_t hash1 = DefaultHash<T1>{}(value.first);
    uint32_t hash2 = DefaultHash<T2>{}(value.second);
    return hash1 ^ (hash2 * 33);
  }
};

}  // namespace BLI
