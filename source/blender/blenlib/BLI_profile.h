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

#pragma once

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool bli_profiling_is_enabled;

typedef struct BLI_ProfileTask {
  uint64_t id;
} BLI_ProfileTask;

#define BLI_PROFILE_DUMMY_ID (~0)

BLI_INLINE bool BLI_profile_is_enabled(void)
{
  return bli_profiling_is_enabled;
}

void _bli_profile_task_begin(BLI_ProfileTask *task, const char *name);
void _bli_profile_task_begin_subtask(BLI_ProfileTask *task,
                                     const char *name,
                                     const BLI_ProfileTask *parent_task);
void _bli_profile_task_end(BLI_ProfileTask *task);

#define BLI_profile_task_begin(task_ptr, name) \
  if (bli_profiling_is_enabled) { \
    _bli_profile_task_begin((task_ptr), (name)); \
  } \
  else { \
    (task_ptr)->id = BLI_PROFILE_DUMMY_ID; \
  } \
  ((void)0)

#define BLI_profile_task_begin_subtask(task_ptr, name, parent_task) \
  if (bli_profiling_is_enabled) { \
    _bli_profile_task_begin_subtask((task_ptr), (name), (parent_task)); \
  } \
  else { \
    (task_ptr)->id = BLI_PROFILE_DUMMY_ID; \
  } \
  ((void)0)

#define BLI_profile_task_end(task_ptr) \
  if (bli_profiling_is_enabled && (task_ptr)->id != BLI_PROFILE_DUMMY_ID) { \
    _bli_profile_task_end(task_ptr); \
  } \
  ((void)0)

#ifdef __cplusplus
}
#endif
