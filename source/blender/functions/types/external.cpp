#include "BLI_lazy_init.hpp"

#include "FN_types.hpp"
#include "FN_cpp.hpp"
#include "FN_llvm.hpp"
#include "DNA_object_types.h"

namespace FN {
namespace Types {

Type *TYPE_object = nullptr;
Type *TYPE_object_list = nullptr;

void INIT_external(Vector<Type *> &types_to_free)
{
  TYPE_object = new Type("Object");
  TYPE_object->add_extension<CPPTypeInfoForType<Object *>>();
  TYPE_object->add_extension<PointerLLVMTypeInfo>(
      [](void *value) { return value; }, [](void *UNUSED(value)) {}, []() { return nullptr; });

  TYPE_object_list = new_list_type(TYPE_object);

  types_to_free.extend({TYPE_object, TYPE_object_list});
}

}  // namespace Types
}  // namespace FN
