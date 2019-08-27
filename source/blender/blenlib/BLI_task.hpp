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
 * This file contains C++ wrappers for BLI_task.h, that make it easier to use.
 */

#pragma once

#include <mutex>

#include "BLI_task.h"
#include "BLI_array_ref.hpp"
#include "BLI_map.hpp"
#include "BLI_range.hpp"

namespace BLI {
namespace Task {

/**
 * Use this when the processing of individual array elements is relatively expensive.
 * The function has to be a callable that takes an element of type T& as input.
 *
 * For debugging/profiling purposes the threading can be disabled.
 */
template<typename T, typename ProcessElement>
static void parallel_array_elements(ArrayRef<T> array,
                                    ProcessElement process_element,
                                    bool use_threading = true)
{
  if (!use_threading) {
    for (T &element : array) {
      process_element(element);
    }
    return;
  }

  TaskParallelSettings settings = {0};
  BLI_parallel_range_settings_defaults(&settings);
  settings.scheduling_mode = TASK_SCHEDULING_DYNAMIC;

  struct ParallelData {
    ArrayRef<T> array;
    ProcessElement &process_element;
  } data = {array, process_element};

  BLI_task_parallel_range(0,
                          array.size(),
                          (void *)&data,
                          [](void *__restrict userdata,
                             const int index,
                             const TaskParallelTLS *__restrict UNUSED(tls)) {
                            ParallelData &data = *(ParallelData *)userdata;
                            T &element = data.array[index];
                            data.process_element(element);
                          },
                          &settings);
}

template<typename T, typename ProcessElement, typename CreateThreadLocal, typename FreeThreadLocal>
static void parallel_array_elements(ArrayRef<T> array,
                                    ProcessElement process_element,
                                    CreateThreadLocal create_thread_local,
                                    FreeThreadLocal free_thread_local,
                                    bool use_threading = true)
{
  using LocalData = decltype(create_thread_local());

  if (!use_threading) {
    LocalData local_data = create_thread_local();
    for (const T &element : array) {
      process_element(element, local_data);
    }
    free_thread_local(local_data);
    return;
  }

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.scheduling_mode = TASK_SCHEDULING_STATIC;
  settings.min_iter_per_thread = 1;

  struct ParallelData {
    ArrayRef<T> array;
    ProcessElement &process_element;
    CreateThreadLocal &create_thread_local;
    Map<int, LocalData> thread_locals;
    std::mutex thread_locals_mutex;
  } data = {array, process_element, create_thread_local, {}, {}};

  BLI_task_parallel_range(
      0,
      array.size(),
      (void *)&data,
      [](void *__restrict userdata, const int index, const TaskParallelTLS *__restrict tls) {
        ParallelData &data = *(ParallelData *)userdata;
        int thread_id = tls->thread_id;

        data.thread_locals_mutex.lock();
        LocalData *local_data_ptr = data.thread_locals.lookup_ptr(thread_id);
        LocalData local_data = (local_data_ptr == nullptr) ? data.create_thread_local() :
                                                             *local_data_ptr;
        if (local_data_ptr == nullptr) {
          data.thread_locals.add_new(thread_id, local_data);
        }
        data.thread_locals_mutex.unlock();

        const T &element = data.array[index];
        data.process_element(element, local_data);
      },
      &settings);

  for (LocalData local_data : data.thread_locals.values()) {
    free_thread_local(local_data);
  }
}

template<typename ProcessRange>
static void parallel_range(Range<uint> total_range,
                           uint chunk_size,
                           ProcessRange process_range,
                           bool use_threading = true)
{
  if (!use_threading) {
    process_range(total_range);
    return;
  }

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  struct ParallelData {
    ChunkedRange<uint> chunks;
    ProcessRange &process_range;
  } data = {ChunkedRange<uint>(total_range, chunk_size), process_range};

  BLI_task_parallel_range(0,
                          data.chunks.chunks(),
                          (void *)&data,
                          [](void *__restrict userdata,
                             const int index,
                             const TaskParallelTLS *__restrict UNUSED(tls)) {
                            ParallelData &data = *(ParallelData *)userdata;
                            Range<uint> range = data.chunks.chunk_range(index);
                            data.process_range(range);
                          },
                          &settings);
}

}  // namespace Task
}  // namespace BLI
