#ifndef __BLI_RAND_CXX_H__
#define __BLI_RAND_CXX_H__

#include "BLI_utildefines.h"

#include <iostream>

namespace BLI {

inline uint32_t hash_from_path_and_line(const char *path, uint32_t line)
{
  uint32_t hash = 5381;
  const char *str = path;
  char c = 0;
  while ((c = *str++)) {
    hash = hash * 37 + c;
  }
  hash = hash ^ ((line + 573259433) * 654188383);
  return hash;
}

}  // namespace BLI

#define BLI_RAND_PER_LINE_UINT32 BLI::hash_from_path_and_line(__FILE__, __LINE__)

#endif /* __BLI_RAND_CXX_H__ */
