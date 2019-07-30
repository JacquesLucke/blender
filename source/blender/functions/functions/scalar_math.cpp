#include <cmath>

#include "scalar_math.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

static SharedFunction get_math_function__one_input(std::string name)
{
  FunctionBuilder builder;
  builder.add_input("Value", GET_TYPE_float());
  builder.add_output("Result", GET_TYPE_float());
  return builder.build(name);
}

static SharedFunction get_math_function__two_inputs(std::string name)
{
  FunctionBuilder builder;
  builder.add_input("A", GET_TYPE_float());
  builder.add_input("B", GET_TYPE_float());
  builder.add_output("Result", GET_TYPE_float());
  return builder.build(name);
}

class AddFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    fn_out.set<float>(0, a + b);
  }
};

class GenAddFloats : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    auto output = builder.CreateFAdd(interface.get_input(0), interface.get_input(1));
    interface.set_output(0, output);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_add_floats)
{
  auto fn = get_math_function__two_inputs("Add Floats");
  // fn->add_body<AddFloats>();
  fn->add_body<GenAddFloats>();
  return fn;
}

class MultiplyFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    fn_out.set<float>(0, a * b);
  }
};

class MultiplyFloatsGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    auto output = builder.CreateFMul(interface.get_input(0), interface.get_input(1));
    interface.set_output(0, output);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_multiply_floats)
{
  auto fn = get_math_function__two_inputs("Multiply Floats");
  fn->add_body<MultiplyFloats>();
  fn->add_body<MultiplyFloatsGen>();
  return fn;
}

class MinFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    fn_out.set<float>(0, (a < b) ? a : b);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_min_floats)
{
  auto fn = get_math_function__two_inputs("Minimum");
  fn->add_body<MinFloats>();
  return fn;
}

class MaxFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    fn_out.set<float>(0, (a < b) ? b : a);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_max_floats)
{
  auto fn = get_math_function__two_inputs("Maximum");
  fn->add_body<MaxFloats>();
  return fn;
}

class MapRange : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float value = fn_in.get<float>(0);
    float from_min = fn_in.get<float>(1);
    float from_max = fn_in.get<float>(2);
    float to_min = fn_in.get<float>(3);
    float to_max = fn_in.get<float>(4);

    float from_range = from_max - from_min;
    float to_range = to_max - to_min;

    float result;
    if (from_range == 0) {
      result = to_min;
    }
    else {
      float t = (value - from_min) / from_range;
      CLAMP(t, 0.0f, 1.0f);
      result = t * to_range + to_min;
    }

    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_map_range)
{
  FunctionBuilder builder;
  builder.add_input("Value", GET_TYPE_float());
  builder.add_input("From Min", GET_TYPE_float());
  builder.add_input("From Max", GET_TYPE_float());
  builder.add_input("To Min", GET_TYPE_float());
  builder.add_input("To Max", GET_TYPE_float());
  builder.add_output("Value", GET_TYPE_float());

  auto fn = builder.build("Map Range");
  fn->add_body<MapRange>();
  return fn;
}

class SinFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, std::sin(a));
  }
};

class SinFloatGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    auto output = builder.CreateSin(interface.get_input(0));
    interface.set_output(0, output);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_sin_float)
{
  auto fn = get_math_function__one_input("Sin");
  fn->add_body<SinFloat>();
  fn->add_body<SinFloatGen>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
