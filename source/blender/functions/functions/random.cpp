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
    uint32_t seed = fn_in.get<int32_t>(0);
    float min = fn_in.get<float>(1);
    float max = fn_in.get<float>(2);
    float result = random_float(seed) * (max - min) + min;
    fn_out.set<float>(0, result);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_random_number)
{
  auto fn = SharedFunction::New("Random Number",
                                Signature(
                                    {
                                        InputParameter("Seed", GET_TYPE_int32()),
                                        InputParameter("Min", GET_TYPE_float()),
                                        InputParameter("Max", GET_TYPE_float()),
                                    },
                                    {
                                        OutputParameter("Value", GET_TYPE_float()),
                                    }));
  fn->add_body<RandomNumber>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
