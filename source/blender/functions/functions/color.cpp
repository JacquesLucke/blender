#include "FN_tuple_call.hpp"
#include "FN_types.hpp"
#include "BLI_lazy_init_cxx.h"

#include "color.hpp"

namespace FN {
namespace Functions {

using namespace Types;

class SeparateColor : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    FN_TUPLE_CALL_NAMED_REF(this, fn_in, fn_out, inputs, outputs);

    rgba_f color = inputs.get<rgba_f>(0, "Color");
    outputs.set<float>(0, "Red", color.r);
    outputs.set<float>(1, "Green", color.g);
    outputs.set<float>(2, "Blue", color.b);
    outputs.set<float>(3, "Alpha", color.a);
  }
};

BLI_LAZY_INIT_REF(Function, GET_FN_separate_color)
{
  FunctionBuilder fn_builder;
  fn_builder.add_input("Color", TYPE_rgba_f);
  fn_builder.add_output("Red", TYPE_float);
  fn_builder.add_output("Green", TYPE_float);
  fn_builder.add_output("Blue", TYPE_float);
  fn_builder.add_output("Alpha", TYPE_float);

  auto fn = fn_builder.build("Separate Color");
  fn->add_body<SeparateColor>();
  return fn;
}

class CombineColor : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    FN_TUPLE_CALL_NAMED_REF(this, fn_in, fn_out, inputs, outputs);

    rgba_f color;
    color.r = inputs.get<float>(0, "Red");
    color.g = inputs.get<float>(1, "Green");
    color.b = inputs.get<float>(2, "Blue");
    color.a = inputs.get<float>(3, "Alpha");
    outputs.set<rgba_f>(0, "Color", color);
  }
};

BLI_LAZY_INIT_REF(Function, GET_FN_combine_color)
{
  FunctionBuilder fn_builder;
  fn_builder.add_input("Red", TYPE_float);
  fn_builder.add_input("Green", TYPE_float);
  fn_builder.add_input("Blue", TYPE_float);
  fn_builder.add_input("Alpha", TYPE_float);
  fn_builder.add_output("Color", TYPE_rgba_f);

  auto fn = fn_builder.build("Combine Color");
  fn->add_body<CombineColor>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
