#include "object_input.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"
#include "DNA_object_types.h"

namespace FN {
namespace Functions {

using namespace Types;

class ObjectLocation : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Object *object = fn_in.get<Object *>(0);
    if (object) {
      float3 position = object->loc;
      fn_out.set<float3>(0, position);
    }
    else {
      fn_out.set<float3>(0, float3());
    }
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_object_location)
{
  FunctionBuilder builder;
  builder.add_input("Object", GET_TYPE_object());
  builder.add_output("Location", GET_TYPE_float3());
  auto fn = builder.build("Object Location");
  fn->add_body<ObjectLocation>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
