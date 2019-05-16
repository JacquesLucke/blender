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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_add_floats)
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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_multiply_floats)
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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_min_floats)
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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_max_floats)
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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_map_range)
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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_sin_float)
{
  auto fn = get_math_function__one_input("Sin");
  fn->add_body<SinFloat>();
  fn->add_body<SinFloatGen>();
  return fn;
}

/* Constant value builders
 *************************************/

template<typename T> class ConstValue : public TupleCallBody {
 private:
  T m_value;

 public:
  ConstValue(T value) : m_value(value)
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    fn_out.set<T>(0, m_value);
  }
};

class ConstInt32Gen : public LLVMBuildIRBody {
 private:
  int32_t m_value;

 public:
  ConstInt32Gen(int32_t value) : m_value(value)
  {
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *constant = builder.getInt32(m_value);
    interface.set_output(0, constant);
  }
};

class ConstFloatGen : public LLVMBuildIRBody {
 private:
  float m_value;

 public:
  ConstFloatGen(float value) : m_value(value)
  {
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *constant = builder.getFloat(m_value);
    interface.set_output(0, constant);
  }
};

class ConstBoolGen : public LLVMBuildIRBody {
 private:
  bool m_value;

 public:
  ConstBoolGen(bool value) : m_value(value)
  {
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *constant = builder.getInt1(m_value);
    interface.set_output(0, constant);
  }
};

static SharedFunction get_output_int32_function(int32_t value)
{
  FunctionBuilder builder;
  builder.add_output("Value", GET_TYPE_int32());
  auto fn = builder.build("Build Value: " + std::to_string(value));
  fn->add_body<ConstValue<int32_t>>(value);
  fn->add_body<ConstInt32Gen>(value);
  return fn;
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_int32_0)
{
  return get_output_int32_function(0);
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_int32_1)
{
  return get_output_int32_function(1);
}

static SharedFunction get_output_float_function(float value)
{
  FunctionBuilder builder;
  builder.add_output("Value", GET_TYPE_float());
  auto fn = builder.build("Build Value: " + std::to_string(value));
  fn->add_body<ConstValue<float>>(value);
  fn->add_body<ConstFloatGen>(value);
  return fn;
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_float_0)
{
  return get_output_float_function(0.0f);
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_float_1)
{
  return get_output_float_function(1.0f);
}

static SharedFunction get_output_bool_function(bool value)
{
  FunctionBuilder builder;
  builder.add_output("Value", GET_TYPE_bool());
  auto fn = builder.build("Build Value");
  fn->add_body<ConstValue<bool>>(value);
  fn->add_body<ConstBoolGen>(value);
  return fn;
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_false)
{
  return get_output_bool_function(false);
}

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_output_true)
{
  return get_output_bool_function(true);
}

}  // namespace Functions
}  // namespace FN
