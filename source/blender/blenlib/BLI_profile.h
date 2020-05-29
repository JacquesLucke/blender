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

#ifndef __BLI_PROFILE_HH__
#define __BLI_PROFILE_HH__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProfilePosition {
  const char *file;
  const char *function;
  int line;
} ProfilePosition;

void bli_profile_begin(const ProfilePosition *scope);
void bli_profile_end();

#define BLI_PROFILE_BEGIN() \
  { \
    static const ProfilePosition profile_position = {__FILE__, __func__, __LINE__}; \
    bli_profile_begin(&profile_position); \
  } \
  ((void)0)

#define BLI_PROFILE_END() bli_profile_end()

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace BLI {
struct ScopedProfile {
  const ProfilePosition *m_position;

  ScopedProfile(const ProfilePosition *position) : m_position(position)
  {
    bli_profile_begin(position);
  }

  ~ScopedProfile()
  {
    bli_profile_end();
  }
};
}  // namespace BLI

#  define BLI_PROFILE_FUNCTION() \
    static const ProfilePosition profile_position = {__FILE__, __func__, __LINE__}; \
    BLI::ScopedProfile scoped_profile(&profile_position); \
    ((void)0)

#endif /* __cplusplus */

#endif /* __BLI_PROFILE_HH__ */
