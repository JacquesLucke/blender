#include "vectors.hpp"
#include "FN_types.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "BLI_lazy_init.hpp"
#include "BLI_math.h"

namespace FN {
namespace Functions {

using namespace Types;

class CombineVectorGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Type *vector_ty = get_llvm_type(GET_TYPE_float3(), builder.getContext());

    llvm::Value *vector = builder.getUndef(vector_ty);
    vector = builder.CreateInsertElement(vector, interface.get_input(0), 0);
    vector = builder.CreateInsertElement(vector, interface.get_input(1), 1);
    vector = builder.CreateInsertElement(vector, interface.get_input(2), 2);
    interface.set_output(0, vector);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_combine_vector)
{
  FunctionBuilder builder;
  builder.add_input("X", GET_TYPE_float());
  builder.add_input("Y", GET_TYPE_float());
  builder.add_input("Z", GET_TYPE_float());
  builder.add_output("Vector", GET_TYPE_float3());

  auto fn = builder.build("Combine Vector");
  fn->add_body<CombineVectorGen>();
  return fn;
}

class SeparateVector : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *vector = interface.get_input(0);
    interface.set_output(0, builder.CreateExtractElement(vector, 0));
    interface.set_output(1, builder.CreateExtractElement(vector, 1));
    interface.set_output(2, builder.CreateExtractElement(vector, 2));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_separate_vector)
{
  FunctionBuilder builder;
  builder.add_input("Vector", GET_TYPE_float3());
  builder.add_output("X", GET_TYPE_float());
  builder.add_output("Y", GET_TYPE_float());
  builder.add_output("Z", GET_TYPE_float());

  auto fn = builder.build("Separate Vector");
  fn->add_body<SeparateVector>();
  return fn;
}

class VectorDistance : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float3 a = fn_in.get<float3>(0);
    float3 b = fn_in.get<float3>(1);
    fn_out.set<float>(0, float3::distance(a, b));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_vector_distance)
{
  FunctionBuilder builder;
  builder.add_input("A", GET_TYPE_float3());
  builder.add_input("B", GET_TYPE_float3());
  builder.add_output("Distance", GET_TYPE_float());

  auto fn = builder.build("Vector Distance");
  fn->add_body<VectorDistance>();
  return fn;
}

static SharedFunction get_math_function__two_inputs(std::string name)
{
  FunctionBuilder builder;
  builder.add_input("A", GET_TYPE_float3());
  builder.add_input("B", GET_TYPE_float3());
  builder.add_output("Result", GET_TYPE_float3());
  return builder.build(name);
}

class AddVectors : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float3 a = fn_in.get<float3>(0);
    float3 b = fn_in.get<float3>(1);
    fn_out.set<float3>(0, a + b);
  }
};

class AddVectorsGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *a = interface.get_input(0);
    llvm::Value *b = interface.get_input(1);
    llvm::Value *result = builder.CreateFAdd(a, b);
    interface.set_output(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_add_vectors)
{
  auto fn = get_math_function__two_inputs("Add Vectors");
  fn->add_body<AddVectors>();
  fn->add_body<AddVectorsGen>();
  return fn;
}

class SubVectors : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float3 a = fn_in.get<float3>(0);
    float3 b = fn_in.get<float3>(1);
    fn_out.set<float3>(0, a - b);
  }
};

class SubVectorsGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *a = interface.get_input(0);
    llvm::Value *b = interface.get_input(1);
    llvm::Value *result = builder.CreateFSub(a, b);
    interface.set_output(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_sub_vectors)
{
  auto fn = get_math_function__two_inputs("Subtract Vectors");
  fn->add_body<SubVectors>();
  fn->add_body<SubVectorsGen>();
  return fn;
}

class CrossProductVectors : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float3 a = fn_in.get<float3>(0);
    float3 b = fn_in.get<float3>(1);
    float3 result;
    cross_v3_v3v3_hi_prec(result, a, b);
    fn_out.set<float3>(0, result);
  }
};

class CrossProductVectorsGen : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    auto a = interface.get_input(0);
    auto b = interface.get_input(1);

    auto a_x = builder.CreateExtractElement(a, 0);
    auto a_y = builder.CreateExtractElement(a, 1);
    auto a_z = builder.CreateExtractElement(a, 2);

    auto b_x = builder.CreateExtractElement(b, 0);
    auto b_y = builder.CreateExtractElement(b, 1);
    auto b_z = builder.CreateExtractElement(b, 2);

    auto mul_ay_bz = builder.CreateFMul(a_y, b_z);
    auto mul_az_by = builder.CreateFMul(a_z, b_y);
    auto result_x = builder.CreateFSub(mul_ay_bz, mul_az_by);

    auto mul_az_bx = builder.CreateFMul(a_z, b_x);
    auto mul_ax_bz = builder.CreateFMul(a_x, b_z);
    auto result_y = builder.CreateFSub(mul_az_bx, mul_ax_bz);

    auto mul_ax_by = builder.CreateFMul(a_x, b_y);
    auto mul_ay_bx = builder.CreateFMul(a_y, b_x);
    auto result_z = builder.CreateFSub(mul_ax_by, mul_ay_bx);

    auto result = static_cast<llvm::Value *>(builder.getUndef(a->getType()));

    result = builder.CreateInsertElement(result, result_x, 0);
    result = builder.CreateInsertElement(result, result_y, 1);
    result = builder.CreateInsertElement(result, result_z, 2);

    interface.set_output(0, result);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_cross_vectors)
{
  auto fn = get_math_function__two_inputs("Cross Product");
  fn->add_body<CrossProductVectors>();
  fn->add_body<CrossProductVectorsGen>();
  return fn;
}

class ReflectVectors : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    float3 a = fn_in.get<float3>(0);
    float3 b = fn_in.get<float3>(1);
    fn_out.set<float3>(0, a.reflected(b.normalized()));
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_reflect_vectors)
{
  auto fn = get_math_function__two_inputs("Reflect Vectors");
  fn->add_body<ReflectVectors>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
