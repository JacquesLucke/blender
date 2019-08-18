#pragma once

#include <functional>

#include "BLI_utildefines.h"

namespace BLI {

template<typename T> class MyHash {
 public:
  uint32_t operator()(const T &value)
  {
    return (uint32_t)std::hash<T>{}(value);
  }
};

}  // namespace BLI
