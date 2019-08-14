#include "simple_conversions.hpp"
#include "FN_types.hpp"
#include "FN_functions.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

template<typename From, typename To> class ImplicitConversion : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    To value = (To)fn_in.relocate_out<From>(0);
    fn_out.move_in<To>(0, value);
  }
};

static SharedFunction get_simple_conversion_function(Type *from_type, Type *to_type)
{
  FunctionBuilder builder;
  builder.add_input("In", from_type);
  builder.add_output("Out", to_type);
  auto name = from_type->name() + " to " + to_type->name();
  return builder.build(name);
}

template<typename From, typename To>
static SharedFunction get_implicit_conversion_function(Type *from_type, Type *to_type)
{
  auto fn = get_simple_conversion_function(from_type, to_type);
  fn->add_body<ImplicitConversion<From, To>>();
  return fn;
}

/* Individual Element Conversion */

BLI_LAZY_INIT(SharedFunction, GET_FN_bool_to_int32)
{
  return get_implicit_conversion_function<bool, int32_t>(GET_TYPE_bool(), GET_TYPE_int32());
}

BLI_LAZY_INIT(SharedFunction, GET_FN_bool_to_float)
{
  return get_implicit_conversion_function<bool, float>(GET_TYPE_bool(), GET_TYPE_float());
}

BLI_LAZY_INIT(SharedFunction, GET_FN_int32_to_float)
{
  return get_implicit_conversion_function<int32_t, float>(GET_TYPE_int32(), GET_TYPE_float());
}

BLI_LAZY_INIT(SharedFunction, GET_FN_int32_to_bool)
{
  return get_implicit_conversion_function<int32_t, bool>(GET_TYPE_int32(), GET_TYPE_bool());
}

BLI_LAZY_INIT(SharedFunction, GET_FN_float_to_int32)
{
  return get_implicit_conversion_function<float, int32_t>(GET_TYPE_float(), GET_TYPE_int32());
}

BLI_LAZY_INIT(SharedFunction, GET_FN_float_to_bool)
{
  return get_implicit_conversion_function<float, bool>(GET_TYPE_float(), GET_TYPE_bool());
}

/* List Conversions */

BLI_LAZY_INIT(SharedFunction, GET_FN_bool_list_to_int32_list)
{
  return to_vectorized_function(GET_FN_bool_to_int32(), {true}, {GET_FN_output_false()});
}

BLI_LAZY_INIT(SharedFunction, GET_FN_bool_list_to_float_list)
{
  return to_vectorized_function(GET_FN_bool_to_float(), {true}, {GET_FN_output_false()});
}

BLI_LAZY_INIT(SharedFunction, GET_FN_int32_list_to_float_list)
{
  return to_vectorized_function(GET_FN_int32_to_float(), {true}, {GET_FN_output_int32_0()});
}

BLI_LAZY_INIT(SharedFunction, GET_FN_int32_list_to_bool_list)
{
  return to_vectorized_function(GET_FN_int32_to_bool(), {true}, {GET_FN_output_int32_0()});
}

BLI_LAZY_INIT(SharedFunction, GET_FN_float_list_to_int32_list)
{
  return to_vectorized_function(GET_FN_float_to_int32(), {true}, {GET_FN_output_float_0()});
}

BLI_LAZY_INIT(SharedFunction, GET_FN_float_list_to_bool_list)
{
  return to_vectorized_function(GET_FN_float_to_bool(), {true}, {GET_FN_output_float_0()});
}

}  // namespace Functions
}  // namespace FN
