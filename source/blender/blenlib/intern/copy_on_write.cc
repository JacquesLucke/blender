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
 */

#include "atomic_ops.h"

#include "BLI_assert.h"
#include "BLI_copy_on_write.h"

#include "MEM_guardedalloc.h"

static int &get_counter(const bCopyOnWrite *cow)
{
  BLI_assert(cow != nullptr);
  return *const_cast<int *>(&cow->user_count);
}

bCopyOnWrite *BLI_cow_new(int user_count)
{
  bCopyOnWrite *cow = MEM_cnew<bCopyOnWrite>(__func__);
  cow->user_count = user_count;
  return cow;
}

void BLI_cow_free(const bCopyOnWrite *cow)
{
  BLI_assert(cow->user_count == 0);
  MEM_freeN(const_cast<bCopyOnWrite *>(cow));
}

void BLI_cow_init(const bCopyOnWrite *cow)
{
  get_counter(cow) = 1;
}

bool BLI_cow_is_mutable(const bCopyOnWrite *cow)
{
  return !BLI_cow_is_shared(cow);
}

bool BLI_cow_is_shared(const bCopyOnWrite *cow)
{
  return cow->user_count >= 2;
}

void BLI_cow_user_add(const bCopyOnWrite *cow)
{
  atomic_fetch_and_add_int32(&get_counter(cow), 1);
}

bool BLI_cow_user_remove(const bCopyOnWrite *cow)
{
  const int new_user_count = atomic_sub_and_fetch_int32(&get_counter(cow), 1);
  BLI_assert(new_user_count >= 0);
  const bool has_no_user_anymore = new_user_count == 0;
  return has_no_user_anymore;
}
