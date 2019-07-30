#include "FN_tuple_call.hpp"
#include "FN_types.hpp"
#include "BLI_lazy_init.hpp"

#include "color.hpp"

namespace FN {
namespace Functions {

using namespace Types;

class SeparateColor : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    rgba_f color = this->get_input<rgba_f>(fn_in, 0, "Color");
    this->set_output<float>(fn_out, 0, "Red", color.r);
    this->set_output<float>(fn_out, 1, "Green", color.g);
    this->set_output<float>(fn_out, 2, "Blue", color.b);
    this->set_output<float>(fn_out, 3, "Alpha", color.a);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_separate_color)
{
  FunctionBuilder fn_builder;
  fn_builder.add_input("Color", GET_TYPE_rgba_f());
  fn_builder.add_output("Red", GET_TYPE_float());
  fn_builder.add_output("Green", GET_TYPE_float());
  fn_builder.add_output("Blue", GET_TYPE_float());
  fn_builder.add_output("Alpha", GET_TYPE_float());

  auto fn = fn_builder.build("Separate Color");
  fn->add_body<SeparateColor>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
