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
    llvm::Type *vector_ty = get_llvm_type(GET_TYPE_fvec3(), builder.getContext());

    llvm::Value *vector = builder.getUndef(vector_ty);
    vector = builder.CreateInsertElement(vector, interface.get_input(0), 0);
    vector = builder.CreateInsertElement(vector, interface.get_input(1), 1);
    vector = builder.CreateInsertElement(vector, interface.get_input(2), 2);
    interface.set_output(0, vector);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_combine_vector)
{
  auto fn = SharedFunction::New("Combine Vector",
                                Signature(
                                    {
                                        InputParameter("X", GET_TYPE_float()),
                                        InputParameter("Y", GET_TYPE_float()),
                                        InputParameter("Z", GET_TYPE_float()),
                                    },
                                    {
                                        OutputParameter("Vector", GET_TYPE_fvec3()),
                                    }));
  fn->add_body(new CombineVectorGen());
  return fn;
}

class SeparateVector : public LLVMBuildIRBody {
  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    llvm::Value *vector = interface.get_input(0);
    interface.set_output(0, builder.CreateExtractValue(vector, 0));
    interface.set_output(1, builder.CreateExtractValue(vector, 1));
    interface.set_output(2, builder.CreateExtractValue(vector, 2));
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_separate_vector)
{
  auto fn = SharedFunction::New("Separate Vector",
                                Signature(
                                    {
                                        InputParameter("Vector", GET_TYPE_fvec3()),
                                    },
                                    {
                                        OutputParameter("X", GET_TYPE_float()),
                                        OutputParameter("Y", GET_TYPE_float()),
                                        OutputParameter("Z", GET_TYPE_float()),
                                    }));
  fn->add_body(new SeparateVector());
  return fn;
}

class VectorDistance : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Vector a = fn_in.get<Vector>(0);
    Vector b = fn_in.get<Vector>(1);
    float distance = len_v3v3((float *)&a, (float *)&b);
    fn_out.set<float>(0, distance);
  }
};

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_vector_distance)
{
  auto fn = SharedFunction::New("Vector Distance",
                                Signature(
                                    {
                                        InputParameter("A", GET_TYPE_fvec3()),
                                        InputParameter("B", GET_TYPE_fvec3()),
                                    },
                                    {
                                        OutputParameter("Distance", GET_TYPE_float()),
                                    }));
  fn->add_body(new VectorDistance());
  return fn;
}

static SharedFunction get_math_function__two_inputs(std::string name)
{
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("A", GET_TYPE_fvec3()),
                                        InputParameter("B", GET_TYPE_fvec3()),
                                    },
                                    {
                                        OutputParameter("Result", GET_TYPE_fvec3()),
                                    }));
  return fn;
}

class AddVectors : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Vector a = fn_in.get<Vector>(0);
    Vector b = fn_in.get<Vector>(1);
    Vector result(a.x + b.x, a.y + b.y, a.z + b.z);
    fn_out.set<Vector>(0, result);
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

LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_add_vectors)
{
  auto fn = get_math_function__two_inputs("Add Vectors");
  fn->add_body(new AddVectors());
  fn->add_body(new AddVectorsGen());
  return fn;
}

}  // namespace Functions
}  // namespace FN
