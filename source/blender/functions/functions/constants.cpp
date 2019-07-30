#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "FN_types.hpp"
#include "BLI_lazy_init.hpp"

#include "constants.hpp"

namespace FN {
namespace Functions {

using namespace Types;

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

BLI_LAZY_INIT(SharedFunction, GET_FN_output_int32_0)
{
  return get_output_int32_function(0);
}

BLI_LAZY_INIT(SharedFunction, GET_FN_output_int32_1)
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

BLI_LAZY_INIT(SharedFunction, GET_FN_output_float_0)
{
  return get_output_float_function(0.0f);
}

BLI_LAZY_INIT(SharedFunction, GET_FN_output_float_1)
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

BLI_LAZY_INIT(SharedFunction, GET_FN_output_false)
{
  return get_output_bool_function(false);
}

BLI_LAZY_INIT(SharedFunction, GET_FN_output_true)
{
  return get_output_bool_function(true);
}

template<uint N> class ConstFloatArrayGen : public LLVMBuildIRBody {
 private:
  std::array<float, N> m_array;
  LLVMTypeInfo &m_type_info;

 public:
  ConstFloatArrayGen(std::array<float, N> array, LLVMTypeInfo &type_info)
      : m_array(array), m_type_info(type_info)
  {
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *output = builder.getUndef(m_type_info.get_type(builder.getContext()));
    for (uint i = 0; i < N; i++) {
      output = builder.CreateInsertElement(output, builder.getFloat(m_array[i]), i);
    }
    interface.set_output(0, output);
  }
};

static SharedFunction get_output_float3_function(float3 vector)
{
  FunctionBuilder builder;
  auto &float3_type = GET_TYPE_float3();
  builder.add_output("Vector", float3_type);
  auto fn = builder.build("Build Vector");
  fn->add_body<ConstValue<float3>>(vector);
  fn->add_body<ConstFloatArrayGen<3>>(vector, float3_type->extension<LLVMTypeInfo>());
  return fn;
}

BLI_LAZY_INIT(SharedFunction, GET_FN_output_float3_0)
{
  return get_output_float3_function(float3(0, 0, 0));
}

BLI_LAZY_INIT(SharedFunction, GET_FN_output_float3_1)
{
  return get_output_float3_function(float3(1, 1, 1));
}

static SharedFunction get_output_rgba_f_function(rgba_f color)
{
  FunctionBuilder builder;
  auto &rgba_f_type = GET_TYPE_rgba_f();
  builder.add_output("RGBA Float", rgba_f_type);
  auto fn = builder.build("Build Color");
  fn->add_body<ConstValue<rgba_f>>(color);
  fn->add_body<ConstFloatArrayGen<4>>(color, rgba_f_type->extension<LLVMTypeInfo>());
  return fn;
}

BLI_LAZY_INIT(SharedFunction, GET_FN_output_magenta)
{
  return get_output_rgba_f_function(rgba_f(1, 0, 1, 1));
}

}  // namespace Functions
}  // namespace FN
