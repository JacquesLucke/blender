#pragma once

#include "BLI_temporary_allocator.h"

namespace BLI {

template<typename T> class MutableArrayRef;

template<typename T> MutableArrayRef<T> temporary_allocate_array(uint size)
{
  void *ptr = BLI_temporary_allocate(sizeof(T) * size);
  return MutableArrayRef<T>((T *)ptr, size);
}

};  // namespace BLI
