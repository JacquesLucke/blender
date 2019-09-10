#include "FN_functions.hpp"

#include "BKE_falloff.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

class PointDistanceFalloff : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float3 point = this->get_input<float3>(fn_in, 0, "Point");
    float min_distance = this->get_input<float>(fn_in, 1, "Min Distance");
    float max_distance = this->get_input<float>(fn_in, 2, "Max Distance");

    FalloffW falloff = new BKE::PointDistanceFalloff(point, min_distance, max_distance);
    fn_out.move_in(0, falloff);
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
    float weight = this->get_input<float>(fn_in, 0, "Weight");

    FalloffW falloff = new BKE::ConstantFalloff(weight);
    fn_out.move_in(0, falloff);
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
    Object *object = fn_in.relocate_out<ObjectW>(0).ptr();
    if (object == nullptr || object->type != OB_MESH) {
      FalloffW fallback = new BKE::ConstantFalloff(1.0f);
      fn_out.move_in(0, fallback);
      return;
    }

    float inner_distance = fn_in.get<float>(1);
    float outer_distance = fn_in.get<float>(2);
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
