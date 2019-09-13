#include "FN_functions.hpp"

#include "BKE_falloff.hpp"

#include "BLI_lazy_init_cxx.h"

namespace FN {
namespace Functions {

using namespace Types;

class PointDistanceFalloff : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    FN_TUPLE_CALL_NAMED_REF(this, fn_in, fn_out, inputs, outputs);

    float3 point = inputs.get<float3>(0, "Point");
    float min_distance = inputs.get<float>(1, "Min Distance");
    float max_distance = inputs.get<float>(2, "Max Distance");

    FalloffW falloff = new BKE::PointDistanceFalloff(point, min_distance, max_distance);
    outputs.move_in(0, "Falloff", falloff);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_point_distance_falloff)
{
  FunctionBuilder builder;
  builder.add_input("Point", TYPE_float3);
  builder.add_input("Min Distance", TYPE_float);
  builder.add_input("Max Distance", TYPE_float);
  builder.add_output("Falloff", TYPE_falloff);

  auto fn = builder.build("Point Distance Falloff");
  fn->add_body<PointDistanceFalloff>();
  return fn;
}

class ConstantFalloff : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    FN_TUPLE_CALL_NAMED_REF(this, fn_in, fn_out, inputs, outputs);

    float weight = inputs.get<float>(0, "Weight");

    FalloffW falloff = new BKE::ConstantFalloff(weight);
    outputs.move_in(0, "Falloff", falloff);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_constant_falloff)
{
  FunctionBuilder builder;
  builder.add_input("Weight", TYPE_float);
  builder.add_output("Falloff", TYPE_falloff);

  auto fn = builder.build("Constant Falloff");
  fn->add_body<ConstantFalloff>();
  return fn;
}

class MeshDistanceFalloff : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    FN_TUPLE_CALL_NAMED_REF(this, fn_in, fn_out, inputs, outputs);

    Object *object = inputs.relocate_out<ObjectW>(0, "Object").ptr();
    if (object == nullptr || object->type != OB_MESH) {
      FalloffW fallback = new BKE::ConstantFalloff(1.0f);
      outputs.move_in(0, "Falloff", fallback);
      return;
    }

    float inner_distance = inputs.get<float>(1, "Inner Distance");
    float outer_distance = inputs.get<float>(2, "Outer Distance");
    FalloffW falloff = new BKE::MeshDistanceFalloff(object, inner_distance, outer_distance);
    fn_out.move_in(0, falloff);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_mesh_distance_falloff)
{
  FunctionBuilder builder;
  builder.add_input("Object", TYPE_object);
  builder.add_input("Inner Distance", TYPE_float);
  builder.add_input("Outer Distance", TYPE_float);
  builder.add_output("Falloff", TYPE_falloff);

  auto fn = builder.build("Mesh Distance Falloff");
  fn->add_body<MeshDistanceFalloff>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
