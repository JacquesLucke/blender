/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_context_stack.hh"

#include "testing/testing.h"

namespace blender::tests {

TEST(context_stack, Basic)
{
  ContextStack context_stack{nullptr};
}

}  // namespace blender::tests
