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

#include "BLI_profile.h"

namespace blender::profile {

class ProfileTaskCPP {
 private:
  ProfileTask task_;

 public:
  ProfileTaskCPP(const char *name)
  {
    BLI_profile_task_begin_named(&task_, name);
  }

  ProfileTaskCPP(const char *name, const ProfileTask *parent_task)
  {
    BLI_profile_task_begin_named_subtask(&task_, name, parent_task);
  }

  ~ProfileTaskCPP()
  {
    BLI_profile_task_end(&task_);
  }
};

}  // namespace blender::profile

#define BLI_PROFILE_SCOPE(name) blender::profile::ProfileTaskCPP profile_task((name))

#define BLI_PROFILE_SCOPE_SUBTASK(name, parent_task_ptr) \
  blender::profile::ProfileTaskCPP profile_task((name), (parent_task_ptr))
