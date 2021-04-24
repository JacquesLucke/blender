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

typedef struct BLI_profile_scope {
  uint64_t id;
} BLI_profile_scope;

void BLI_profile_scope_begin(BLI_profile_scope *scope, const char *name);
void BLI_profile_scope_begin_subthread(BLI_profile_scope *scope,
                                       const BLI_profile_scope *parent_scope,
                                       const char *name);
void BLI_profile_scope_end(const BLI_profile_scope *scope);

void BLI_profile_enable(void);
void BLI_profile_disable(void);
bool BLI_profile_is_enabled(void);
void BLI_profile_clear(void);

#ifdef __cplusplus
}
#endif
