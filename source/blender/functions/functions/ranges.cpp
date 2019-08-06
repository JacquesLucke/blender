#include "FN_core.hpp"
#include "FN_functions.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

class FloatRange : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    int amount = this->get_input<int>(fn_in, 0, "Amount");
    float start = this->get_input<float>(fn_in, 1, "Start");
    float step = this->get_input<float>(fn_in, 2, "Step");

    if (amount < 0) {
      amount = 0;
    }

    auto list = SharedList::New(GET_TYPE_float());
    list->reserve_and_set_size(amount);
    auto list_ref = list->as_array_ref<float>();

    float value = start;
    for (uint i = 0; i < amount; i++) {
      list_ref[i] = value;
      value += step;
    }

    fn_out.move_in(0, list);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_float_range)
{
  FunctionBuilder builder;
  builder.add_input("Amount", GET_TYPE_int32());
  builder.add_input("Start", GET_TYPE_float());
  builder.add_input("Step", GET_TYPE_float());
  builder.add_output("List", GET_TYPE_float_list());

  auto fn = builder.build("Float Range");
  fn->add_body<FloatRange>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
