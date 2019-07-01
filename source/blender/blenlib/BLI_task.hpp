#pragma once

#include "BLI_task.h"
#include "BLI_array_ref.hpp"

namespace BLI {
namespace Task {

/**
 * Use this when the processing of individual array elements is relatively expensive.
 * The function has to be a callable that takes an element of type T& as input.
 *
 * For debugging/profiling purposes the threading can be disabled.
 */
template<typename T, typename Func>
static void parallel_array_elements(ArrayRef<T> array, Func function, bool use_threading = false)
{
  if (!use_threading) {
    for (T &element : array) {
      function(element);
    }
    return;
  }

  ParallelRangeSettings settings = {0};
  BLI_parallel_range_settings_defaults(&settings);
  settings.scheduling_mode = TASK_SCHEDULING_DYNAMIC;

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
