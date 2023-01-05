/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_local_allocator.hh"

namespace blender {

LocalAllocatorSet::LocalAllocatorSet()
    : allocator_by_thread_([this]() { return LocalAllocator{*this}; })
{
}

LocalAllocatorSet::~LocalAllocatorSet() = default;

LocalAllocator::LocalAllocator(LocalAllocatorSet &owner_set) : owner_set_(owner_set)
{
  for (const int64_t i : IndexRange(small_stacks_.size())) {
    BufferStack &buffer_stack = small_stacks_[i];
    buffer_stack.element_size = 8 * (i + 1);
    buffer_stack.alignment = power_of_2_min_u(buffer_stack.element_size);
  }
}

LocalAllocator::~LocalAllocator() = default;

}  // namespace blender
