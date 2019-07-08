#include "random.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

static uint32_t random_int(uint32_t x)
{
  x = (x << 13) ^ x;
  return x * (x * x * 15731 + 789221) + 1376312589;
}

static float random_float(uint32_t x)
{
  x = random_int(x);
  return (float)x / 4294967296.0f;
}

class RandomNumber : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float seed = fn_in.get<float>(0);
    float min = fn_in.get<float>(1);
    float max = fn_in.get<float>(2);
    float result = random_float(float_as_uint(seed)) * (max - min) + min;
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_random_number)
{
  FunctionBuilder builder;
  builder.add_input("Seed", GET_TYPE_float());
  builder.add_input("Min", GET_TYPE_float());
  builder.add_input("Max", GET_TYPE_float());
  builder.add_output("Value", GET_TYPE_float());

  auto fn = builder.build("Random Number");
  fn->add_body<RandomNumber>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
