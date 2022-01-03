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

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"

#include "DNA_copy_on_write.h"

#ifdef __cplusplus
extern "C" {
#endif

bCopyOnWrite *BLI_cow_new(int user_count);
void BLI_cow_free(const bCopyOnWrite *cow);

void BLI_cow_init(const bCopyOnWrite *cow, int user_count);

bool BLI_cow_is_shared(const bCopyOnWrite *cow);
bool BLI_cow_is_mutable(const bCopyOnWrite *cow);
bool BLI_cow_is_zero(const bCopyOnWrite *cow);

void BLI_cow_user_add(const bCopyOnWrite *cow);
bool BLI_cow_user_remove(const bCopyOnWrite *cow) ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif
