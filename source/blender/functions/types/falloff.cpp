#include "FN_types.hpp"
#include "FN_cpp.hpp"
#include "FN_llvm.hpp"

#include "BKE_falloff.hpp"

namespace FN {
namespace Types {

Type *TYPE_falloff = nullptr;
Type *TYPE_falloff_list = nullptr;

void INIT_falloff(Vector<Type *> &types_to_free)
{
  TYPE_falloff = new Type("Falloff");
  TYPE_falloff->add_extension<CPPTypeInfoForType<FalloffW>>();
  TYPE_falloff->add_extension<UniquePointerLLVMTypeInfo<BKE::Falloff>>();

  TYPE_falloff_list = new_list_type(TYPE_falloff);

  types_to_free.extend({TYPE_falloff, TYPE_falloff_list});
}

}  // namespace Types
}  // namespace FN
