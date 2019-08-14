#include "external.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_cpp.hpp"
#include "FN_llvm.hpp"
#include "DNA_object_types.h"

namespace FN {
namespace Types {

BLI_LAZY_INIT(Type *, GET_TYPE_object)
{
  Type *type = new Type("Object");
  type->add_extension<CPPTypeInfoForType<Object *>>();
  type->add_extension<PointerLLVMTypeInfo>(
      [](void *value) { return value; }, [](void *UNUSED(value)) {}, []() { return nullptr; });
  return type;
}

}  // namespace Types
}  // namespace FN
