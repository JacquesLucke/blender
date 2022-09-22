/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "atomic_ops.h"

#include "BLI_assert.h"
#include "BLI_copy_on_write.hh"

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
