#include "BLI_lazy_init_cxx.h"

#include "FN_tuple_call.hpp"
#include "FN_types.hpp"

#include "comparisons.hpp"

namespace FN {
namespace Functions {

using namespace Types;

template<typename T> class LessThan : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const
  {
    T a = fn_in.get<T>(0);
    T b = fn_in.get<T>(1);
    fn_out.set<bool>(0, a < b);
  }
};

BLI_LAZY_INIT_REF(Function, GET_FN_less_than_float)
{
  FunctionBuilder builder;
  builder.add_input("A", TYPE_float);
  builder.add_input("B", TYPE_float);
  builder.add_output("A < B", TYPE_bool);

  auto fn = builder.build("Less Than (float)");
  fn->add_body<LessThan<float>>();
  return fn;
}

BLI_LAZY_INIT_REF(Function, GET_FN_less_than_int32)
{
  FunctionBuilder builder;
  builder.add_input("A", TYPE_int32);
  builder.add_input("B", TYPE_int32);
  builder.add_output("A < B", TYPE_bool);

  auto fn = builder.build("Less Than (int32)");
  fn->add_body<LessThan<int32_t>>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
