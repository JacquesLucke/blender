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

}  // namespace Functions
}  // namespace FN
