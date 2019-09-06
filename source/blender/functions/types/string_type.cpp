#include "BLI_lazy_init.hpp"

#include "FN_types.hpp"
#include "FN_cpp.hpp"
#include "FN_llvm.hpp"
#include "DNA_object_types.h"

namespace FN {
namespace Types {

Type *TYPE_string = nullptr;
Type *TYPE_string_list = nullptr;

void INIT_string(Vector<Type *> &types_to_free)
{
  TYPE_string = new Type("String");
  TYPE_string->add_extension<CPPTypeInfoForType<StringW>>();
  TYPE_string->add_extension<UniquePointerLLVMTypeInfo<std::string>>();

  TYPE_string_list = new_list_type(TYPE_string);

  types_to_free.extend({TYPE_string, TYPE_string_list});
}

}  // namespace Types
}  // namespace FN
