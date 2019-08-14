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
  builder.add_input("Value", TYPE_float);
  builder.add_output("Result", TYPE_float);
  return builder.build(name);
}

static SharedFunction get_math_function__two_inputs(std::string name)
{
  FunctionBuilder builder;
  builder.add_input("A", TYPE_float);
  builder.add_input("B", TYPE_float);
  builder.add_output("Result", TYPE_float);
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

class AddFloatsGen : public LLVMBuildIRBody {
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
  fn->add_body<AddFloats>();
  fn->add_body<AddFloatsGen>();
  return fn;
}

class SubFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    fn_out.set<float>(0, a - b);
  }
};

class SubFloatsGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    auto output = builder.CreateFSub(interface.get_input(0), interface.get_input(1));
    interface.set_output(0, output);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_sub_floats)
{
  auto fn = get_math_function__two_inputs("Sub Floats");
  fn->add_body<SubFloats>();
  fn->add_body<SubFloatsGen>();
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

class DivideFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = 0.0f;
    if (b == 0.0f) {
      result = 0.0f;
    }
    else {
      result = a / b;
    }
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_divide_floats)
{
  auto fn = get_math_function__two_inputs("Divide Floats");
  fn->add_body<DivideFloats>();
  return fn;
}

class PowerFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = 0.0f;
    float integral_part = 0.0f;
    if (a != 0.0f && (a > 0.0f || modff(b, &integral_part) == 0.0f)) {
      result = powf(a, b);
    }
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_power_floats)
{
  auto fn = get_math_function__two_inputs("Power Floats");
  fn->add_body<PowerFloats>();
  return fn;
}

class LogarithmFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = 0.0f;
    if (a > 0.0f && b > 0.0f && b != 1.0f) {
      result = logf(a) / logf(b);
    }
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_log_floats)
{
  auto fn = get_math_function__two_inputs("Logarithm");
  fn->add_body<LogarithmFloats>();
  return fn;
}

class SqrtFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, sasqrtf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_sqrt_float)
{
  auto fn = get_math_function__one_input("Square Root");
  fn->add_body<SqrtFloat>();
  return fn;
}

class AbsFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, fabsf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_abs_float)
{
  auto fn = get_math_function__one_input("Absolute Float");
  fn->add_body<AbsFloat>();
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

class CosFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, std::cos(a));
  }
};

class CosFloatGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    auto output = builder.CreateCos(interface.get_input(0));
    interface.set_output(0, output);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_cos_float)
{
  auto fn = get_math_function__one_input("Cos");
  fn->add_body<CosFloat>();
  fn->add_body<CosFloatGen>();
  return fn;
}

class TanFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float result = 0.0f;
    if (std::isfinite(a)) {
      result = tanf(a);
    }
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_tan_float)
{
  auto fn = get_math_function__one_input("Tan");
  fn->add_body<TanFloat>();
  return fn;
}

class ArcsineFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, saasinf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_asin_float)
{
  auto fn = get_math_function__one_input("Arcsine");
  fn->add_body<ArcsineFloat>();
  return fn;
}

class ArccosineFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, saacosf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_acos_float)
{
  auto fn = get_math_function__one_input("Arccosine");
  fn->add_body<ArccosineFloat>();
  return fn;
}

class ArctangentFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, atanf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_atan_float)
{
  auto fn = get_math_function__one_input("Arctangent");
  fn->add_body<ArctangentFloat>();
  return fn;
}

class Arctangent2Floats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    fn_out.set<float>(0, atan2f(b, a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_atan2_floats)
{
  auto fn = get_math_function__two_inputs("Arctangent2");
  fn->add_body<Arctangent2Floats>();
  return fn;
}

class ModuloFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = 0.0f;
    if (std::isfinite(a) && b != 0.0f) {
      result = fmodf(a, b);
    }
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_mod_floats)
{
  auto fn = get_math_function__two_inputs("Modulo Floats");
  fn->add_body<ModuloFloats>();
  return fn;
}

class FractFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float intpt = 0.0f;
    fn_out.set<float>(0, modff(a, &intpt));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_fract_float)
{
  auto fn = get_math_function__one_input("Fract Float");
  fn->add_body<FractFloat>();
  return fn;
}

class CeilFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, ceilf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_ceil_float)
{
  auto fn = get_math_function__one_input("Ceil Float");
  fn->add_body<CeilFloat>();
  return fn;
}

class FloorFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, floorf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_floor_float)
{
  auto fn = get_math_function__one_input("Floor Float");
  fn->add_body<FloorFloat>();
  return fn;
}

class RoundFloat : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    fn_out.set<float>(0, roundf(a));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_round_float)
{
  auto fn = get_math_function__one_input("Round Float");
  fn->add_body<RoundFloat>();
  return fn;
}

class SnapFloats : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = a;
    if (b != 0.0f) {
      double a_d = a;
      double b_d = b;
      result = static_cast<float>(std::ceil(a_d / b_d - 0.5) * b_d);
    }
    fn_out.set<float>(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_snap_floats)
{
  auto fn = get_math_function__two_inputs("Snap Floats");
  fn->add_body<SnapFloats>();
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
  builder.add_input("Value", TYPE_float);
  builder.add_input("From Min", TYPE_float);
  builder.add_input("From Max", TYPE_float);
  builder.add_input("To Min", TYPE_float);
  builder.add_input("To Max", TYPE_float);
  builder.add_output("Value", TYPE_float);

  auto fn = builder.build("Map Range");
  fn->add_body<MapRange>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
