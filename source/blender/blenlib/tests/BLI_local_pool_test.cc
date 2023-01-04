/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_local_pool.hh"
#include "BLI_strict_flags.h"

#include "testing/testing.h"

namespace blender::tests {

TEST(local_pool, Test)
{
  LocalPool pool;

  std::cout << pool.allocate(30000, 8) << "\n";
}

}  // namespace blender::tests
