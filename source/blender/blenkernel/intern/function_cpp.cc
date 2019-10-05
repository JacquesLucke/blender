#include "BKE_function_cpp.h"
#include "BKE_types_cpp.h"
#include "BKE_generic_array_ref.h"

namespace BKE {

FunctionCPP::FunctionCPP()
{
  SignatureBuilderCPP signature;
  this->signature(signature);
  m_signature = std::move(signature.m_data);
}

FunctionCPP::~FunctionCPP()
{
}

class AddFloatsFunction : public FunctionCPP {
  void signature(SignatureBuilderCPP &signature) override
  {
    signature.add_input("A", get_type_cpp<float>());
    signature.add_input("B", get_type_cpp<float>());
    signature.add_output("Result", get_type_cpp<float>());
  }

  void call(TupleRef &fn_in, TupleRef &fn_out) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = a + b;
    fn_out.set<float>(0, result);
  }
};

class AddFloatsArray : public FunctionCPP {
  void signature(SignatureBuilderCPP &signature) override
  {
    signature.add_input("A", get_generic_array_ref_cpp_type(get_type_cpp<float>()));
    signature.add_output("B", get_type_cpp<float>());
  }

  void call(TupleRef &fn_in, TupleRef &fn_out) const override
  {
    ArrayRef<float> values = fn_in.copy_out<GenericArrayRef>(0).get_ref<float>();
    float sum = 0.0f;
    for (float value : values) {
      sum += value;
    }
    fn_out.set<float>(0, sum);
  }
};

}  // namespace BKE