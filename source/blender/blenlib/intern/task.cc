/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"
#include "BLI_vector.hh"

namespace blender::threading {

thread_local RawVector<FunctionRef<void()>> blocking_hint_receivers;

void blocking_compute_hint()
{
  for (int64_t i = blocking_hint_receivers.size() - 1; i >= 0; i--) {
    const FunctionRef<void()> fn = blocking_hint_receivers[i];
    fn();
  }
}

void push_blocking_hint_receiver(const FunctionRef<void()> fn)
{
  blocking_hint_receivers.append(fn);
}

void pop_block_hint_receiver()
{
  blocking_hint_receivers.pop_last();
}

}  // namespace blender::threading
