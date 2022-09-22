/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_assert.h"
#include "BLI_compiler_attrs.h"
#include "BLI_utility_mixins.hh"

#include "DNA_copy_on_write.h"

#ifdef __cplusplus
extern "C" {
#endif

bCopyOnWrite *BLI_cow_new(int user_count);
void BLI_cow_free(const bCopyOnWrite *cow);

void BLI_cow_init(const bCopyOnWrite *cow);

bool BLI_cow_is_shared(const bCopyOnWrite *cow);
bool BLI_cow_is_mutable(const bCopyOnWrite *cow);

void BLI_cow_user_add(const bCopyOnWrite *cow);
bool BLI_cow_user_remove(const bCopyOnWrite *cow) ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif
