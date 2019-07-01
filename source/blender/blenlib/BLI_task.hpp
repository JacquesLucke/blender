#pragma once

#include "BLI_task.h"
#include "BLI_array_ref.hpp"

namespace BLI {
namespace Task {

template<typename T, typename Func>
static void parallel_array(ArrayRef<T> array, Func function, ParallelRangeSettings &settings)
{
  struct ParallelData {
    ArrayRef<T> array;
    Func &function;
  } data = {array, function};

  BLI_task_parallel_range(0,
                          array.size(),
                          (void *)&data,
                          [](void *__restrict userdata,
                             const int index,
                             const ParallelRangeTLS *__restrict UNUSED(tls)) {
                            ParallelData &data = *(ParallelData *)userdata;
                            T &element = data.array[index];
                            data.function(element);
                          },
                          &settings);
}

}  // namespace Task
}  // namespace BLI
