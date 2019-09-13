#include "FN_core.hpp"
#include "FN_functions.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init_cxx.h"

namespace FN {
namespace Functions {

using namespace Types;

class FloatRange : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    FN_TUPLE_CALL_NAMED_REF(this, fn_in, fn_out, inputs, outputs);

    int amount = inputs.get<int>(0, "Amount");
    float start = inputs.get<float>(1, "Start");
    float step = inputs.get<float>(2, "Step");

    if (amount < 0) {
      amount = 0;
    }

    auto list = SharedList::New(TYPE_float);
    list->reserve_and_set_size(amount);
    auto list_ref = list->as_array_ref<float>();

    float value = start;
    for (uint i = 0; i < amount; i++) {
      list_ref[i] = value;
      value += step;
    }

    outputs.move_in(0, "List", list);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_float_range)
{
  FunctionBuilder builder;
  builder.add_input("Amount", TYPE_int32);
  builder.add_input("Start", TYPE_float);
  builder.add_input("Step", TYPE_float);
  builder.add_output("List", TYPE_float_list);

  auto fn = builder.build("Float Range");
  fn->add_body<FloatRange>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
