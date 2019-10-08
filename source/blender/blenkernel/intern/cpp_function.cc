#include "BKE_cpp_function.h"
#include "BKE_cpp_types.h"
#include "BKE_generic_array_ref.h"

namespace BKE {

CPPFunction::CPPFunction()
{
  SignatureBuilderCPP signature;
  this->signature(signature);
  m_signature = std::move(signature.m_data);
}

CPPFunction::~CPPFunction()
{
}

class AddFloatsFunction : public CPPFunction {
  void signature(SignatureBuilderCPP &signature) override
  {
    signature.add_input("A", GET_TYPE<float>());
    signature.add_input("B", GET_TYPE<float>());
    signature.add_output("Result", GET_TYPE<float>());
  }

  void call(TupleRef &fn_in, TupleRef &fn_out) const override
  {
    float a = fn_in.get<float>(0);
    float b = fn_in.get<float>(1);
    float result = a + b;
    fn_out.set<float>(0, result);
  }
};

class AddFloatsArray : public CPPFunction {
  void signature(SignatureBuilderCPP &signature) override
  {
    signature.add_input("A", GET_TYPE_generic_array_ref(GET_TYPE<float>()));
    signature.add_output("B", GET_TYPE<float>());
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