/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "atomic_ops.h"

#include "BLI_assert.h"
#include "BLI_copy_on_write.hh"

#include "MEM_guardedalloc.h"

using blender::FunctionRef;

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

void *BLI_cow_ensure_mutable(bCopyOnWrite **cow_p,
                             const void *old_value,
                             FunctionRef<void *(const void *)> copy_fn,
                             FunctionRef<void(void *)> free_fn)
{
  return BLI_cow_ensure_mutable(
      const_cast<const bCopyOnWrite **>(cow_p), old_value, copy_fn, free_fn);
}

void *BLI_cow_ensure_mutable(const bCopyOnWrite **cow_p,
                             const void *old_value,
                             const FunctionRef<void *(const void *)> copy_fn,
                             const FunctionRef<void(void *)> free_fn)
{
  BLI_assert(cow_p != nullptr);
  const bCopyOnWrite *cow = *cow_p;
  if (old_value == nullptr) {
    return nullptr;
  }
  if (cow == nullptr) {
    return const_cast<void *>(old_value);
  }
  if (BLI_cow_is_mutable(cow)) {
    return const_cast<void *>(old_value);
  }
  void *new_value = copy_fn(old_value);
  if (BLI_cow_user_remove(cow)) {
    free_fn(const_cast<void *>(old_value));
    BLI_cow_init(cow);
  }
  else {
    *cow_p = BLI_cow_new(1);
  }
  return new_value;
}
