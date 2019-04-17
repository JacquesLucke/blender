#include "simple_conversions.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

static SharedFunction get_simple_conversion_function(SharedType &from_type, SharedType &to_type)
{
  auto name = from_type->name() + " to " + to_type->name();
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("In", from_type),
                                    },
                                    {
                                        OutputParameter("Out", to_type),
                                    }));
  return fn;
}

class Int32ToFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    int32_t value = fn_in.get<int32_t>(0);
    fn_out.set<float>(0, (float)value);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_int32_to_float)
{
  auto fn = get_simple_conversion_function(GET_TYPE_int32(), GET_TYPE_float());
  fn->add_body(new Int32ToFloat());
  return fn;
}

class FloatToInt32 : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float value = fn_in.get<float>(0);
    fn_out.set<int32_t>(0, (int32_t)value);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_float_to_int32)
{
  auto fn = get_simple_conversion_function(GET_TYPE_float(), GET_TYPE_int32());
  fn->add_body(new FloatToInt32());
  return fn;
}

}  // namespace Functions
}  // namespace FN
