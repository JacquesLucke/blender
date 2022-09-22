/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_copy_on_write.h"

namespace blender {

class bCopyOnWrite : private NonCopyable, NonMovable {
 private:
  ::bCopyOnWrite base_;

 public:
  bCopyOnWrite()
  {
    BLI_cow_init(&base_);
  }

  ~bCopyOnWrite()
  {
    BLI_assert(this->is_mutable());
  }

  bool is_shared() const
  {
    return BLI_cow_is_shared(&base_);
  }

  bool is_mutable() const
  {
    return BLI_cow_is_mutable(&base_);
  }

  void user_add() const
  {
    BLI_cow_user_add(&base_);
  }

  bool user_remove() const ATTR_WARN_UNUSED_RESULT
  {
    return BLI_cow_user_remove(&base_);
  }
};

}  // namespace blender
