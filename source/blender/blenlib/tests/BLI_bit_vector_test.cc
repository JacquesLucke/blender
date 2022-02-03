/* Apache License, Version 2.0 */

#include "BLI_bit_vector.hh"
#include "BLI_exception_safety_test_utils.hh"
#include "BLI_strict_flags.h"

#include "testing/testing.h"

namespace blender::tests {

TEST(bit_vector, DefaultConstructor)
{
  BitVector vec;
}

}  // namespace blender::tests
