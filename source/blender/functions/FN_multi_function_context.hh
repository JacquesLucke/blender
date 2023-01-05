/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * An #MFContext is passed along with every call to a multi-function. Right now it does nothing,
 * but it can be used for the following purposes:
 * - Pass debug information up and down the function call stack.
 * - Pass reusable memory buffers to sub-functions to increase performance.
 * - Pass cached data to called functions.
 */

#include "BLI_local_allocator.hh"
#include "BLI_utildefines.h"

namespace blender::fn {

class MFContext;

class MFContextBuilder {
 private:
  std::unique_ptr<LocalAllocatorSet> allocator_set_;
  LocalAllocator *allocator_;

  friend MFContext;

 public:
  MFContextBuilder(LocalAllocator *allocator = nullptr)
  {
    if (allocator) {
      allocator_ = allocator;
    }
    else {
      allocator_set_ = std::make_unique<LocalAllocatorSet>();
      allocator_ = &allocator_set_->local();
    }
  }
};

class MFContext {
 private:
  MFContextBuilder &builder_;

 public:
  MFContext(MFContextBuilder &builder) : builder_(builder)
  {
  }

  LocalAllocator &allocator()
  {
    return *builder_.allocator_;
  }
};

}  // namespace blender::fn
