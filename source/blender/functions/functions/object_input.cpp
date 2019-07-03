#include "object_input.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"
#include "DNA_object_types.h"

namespace FN {
namespace Functions {

using namespace Types;

class ObjectTransforms : public TupleCallBody {
 private:
  Object *m_object;

 public:
  ObjectTransforms(Object *object) : m_object(object)
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    if (m_object) {
      float3 position = m_object->loc;
      fn_out.set<float3>(0, position);
    }
    else {
      fn_out.set<float3>(0, float3());
    }
  }
};

class ObjectTransformsDependency : public DependenciesBody {
 private:
  Object *m_object;

 public:
  ObjectTransformsDependency(Object *object) : m_object(object)
  {
  }

  void dependencies(Dependencies &deps) const override
  {
    deps.add_object_transform_dependency(m_object);
  }
};

SharedFunction GET_FN_object_location(Object *object)
{
  FunctionBuilder builder;
  builder.add_output("Location", GET_TYPE_float3());
  auto fn = builder.build("Object Transforms");
  fn->add_body<ObjectTransforms>(object);
  fn->add_body<ObjectTransformsDependency>(object);
  return fn;
}

}  // namespace Functions
}  // namespace FN
