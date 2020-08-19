#include "BLI_utildefines.h"
#include "testing/testing.h"

namespace blender::tests {

enum TestExceptionSafetyTypeOptions : uint32_t {
  ThrowDuringCopyConstruction = 1u << 0,
  ThrowDuringMoveConstruction = 1u << 1,
  ThrowDuringCopyAssignment = 1u << 2,
  ThrowDuringMoveAssignment = 1u << 3,
};

struct TestExceptionSafetyType {
  bool is_alive;
  uint32_t options;

  TestExceptionSafetyType() : is_alive(true), options(0)
  {
  }

  TestExceptionSafetyType(const TestExceptionSafetyType &other) : is_alive(true), options(0)
  {
    if (other.options & ThrowDuringCopyConstruction) {
      throw std::runtime_error("");
    }
  }

  TestExceptionSafetyType(TestExceptionSafetyType &&other) : is_alive(true), options(0)
  {
    if (other.options & ThrowDuringMoveConstruction) {
      throw std::runtime_error("");
    }
  }

  TestExceptionSafetyType &operator=(const TestExceptionSafetyType &other)
  {
    if (options & ThrowDuringCopyAssignment) {
      throw std::runtime_error("");
    }
    if (other.options & ThrowDuringCopyAssignment) {
      throw std::runtime_error("");
    }
    return *this;
  }

  TestExceptionSafetyType &operator=(TestExceptionSafetyType &&other)
  {
    if (options & ThrowDuringMoveAssignment) {
      throw std::runtime_error("");
    }
    if (other.options & ThrowDuringMoveAssignment) {
      throw std::runtime_error("");
    }
    return *this;
  }

  ~TestExceptionSafetyType()
  {
    EXPECT_TRUE(is_alive);
    is_alive = false;
  }
};

}  // namespace blender::tests
