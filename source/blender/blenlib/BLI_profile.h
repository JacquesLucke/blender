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

struct BLI_profile_scope {
  const char *name;
  int64_t begin_time;
  uint64_t id;
  uint64_t parent_id;
};

void BLI_profile_scope_begin(BLI_profile_scope *scope, const char *name);
void BLI_profile_scope_begin_subthread(BLI_profile_scope *scope,
                                       const BLI_profile_scope *parent_scope,
                                       const char *name);
void BLI_profile_scope_end(const BLI_profile_scope *scope);

void BLI_profile_enable();
void BLI_profile_disable();
void BLI_profile_clear();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender::profile {
class ProfileScope {
 private:
  BLI_profile_scope scope;

 public:
  ProfileScope(const char *name)
  {
    BLI_profile_scope_begin(&scope, name);
  }

  ~ProfileScope()
  {
    BLI_profile_scope_end(&scope);
  }
};
}  // namespace blender::profile

#  define BLI_SCOPED_PROFILE(name) blender::profile::ProfileScope profile_scope(name)

#endif
