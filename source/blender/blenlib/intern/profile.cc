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

#include <atomic>
#include <chrono>

#include "BLI_profile.h"
#include "BLI_stack.hh"
#include "BLI_vector.hh"

namespace blender::profile {

using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

struct ProfiledScope {
  const char *name;
  TimePoint start_time;
  TimePoint end_time;
  uint64_t id;
  uint64_t parent_id;
};

static thread_local Stack<uint64_t, 0, RawAllocator> scope_ids_stack;
static thread_local Vector<ProfiledScope, 0, RawAllocator> profiled_scopes;

static uint64_t get_unique_session_id()
{
  /* TODO: Allow getting ids without synchronizing threads for every id. */
  static std::atomic<uint64_t> id = 1;
  return id++;
}

static void profile_scope_begin(BLI_profile_scope *scope, const char *name)
{
  scope->name = name;
  scope->start_time = Clock::now().time_since_epoch().count();
  scope->id = get_unique_session_id();
  scope->parent_id = scope_ids_stack.peek_default(0);
  scope_ids_stack.push(scope->id);
}

static void profile_scope_begin_subthread(BLI_profile_scope *scope,
                                          const BLI_profile_scope *parent_scope,
                                          const char *name)
{
  scope->name = name;
  scope->id = get_unique_session_id();
  scope->parent_id = parent_scope->id;
  scope->start_time = Clock::now().time_since_epoch().count();
  scope_ids_stack.push(scope->id);
}

static void profile_scope_end(const BLI_profile_scope *scope)
{
  TimePoint end_time = Clock::now();
  BLI_assert(scope_ids_stack.peek() == scope->id);
  scope_ids_stack.pop();
  TimePoint start_time = TimePoint(Duration(scope->start_time));

  ProfiledScope profiled_scope;
  profiled_scope.name = scope->name;
  profiled_scope.start_time = start_time;
  profiled_scope.end_time = end_time;
  profiled_scope.id = scope->id;
  profiled_scope.parent_id = scope->parent_id;
  profiled_scopes.append(profiled_scope);
  std::cout << profiled_scope.name << ": "
            << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()
            << " us\n";
}

}  // namespace blender::profile

void BLI_profile_scope_begin(BLI_profile_scope *scope, const char *name)
{
  blender::profile::profile_scope_begin(scope, name);
}

void BLI_profile_scope_begin_subthread(BLI_profile_scope *scope,
                                       const BLI_profile_scope *parent_scope,
                                       const char *name)
{
  blender::profile::profile_scope_begin_subthread(scope, parent_scope, name);
}

void BLI_profile_scope_end(const BLI_profile_scope *scope)
{
  blender::profile::profile_scope_end(scope);
}
