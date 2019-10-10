#include "BKE_node_functions.h"
#include "BKE_generic_array_ref.h"

#include "BLI_math_cxx.h"
#include "BLI_lazy_init_cxx.h"
#include "BLI_string_map.h"
#include "BLI_array_or_single_ref.h"

namespace BKE {

using BLI::ArrayOrSingleRef;
using BLI::float3;

class MultiFunction {
 private:
  Vector<CPPType *> m_single_input_types;
  Vector<CPPType *> m_single_output_types;
  Vector<std::string> m_single_input_names;
  Vector<std::string> m_single_output_names;

 public:
  class Signature {
   private:
    MultiFunction &m_function;

    Signature(MultiFunction &function) : m_function(function)
    {
    }

    friend MultiFunction;

   public:
    template<typename T> void single_input(StringRef name)
    {
      m_function.m_single_input_names.append(name);
      m_function.m_single_input_types.append(&GET_TYPE<T>());
    }

    template<typename T> void single_output(StringRef name)
    {
      m_function.m_single_output_names.append(name);
      m_function.m_single_output_types.append(&GET_TYPE<T>());
    }
  };

  class Inputs {
   public:
    template<typename T> ArrayOrSingleRef<T> get(uint index, StringRef name);
  };

  class Outputs {
   public:
    template<typename T> MutableArrayRef<T> get(uint index, StringRef name);
  };

  virtual void signature(Signature &signature) const = 0;
  virtual void call(ArrayRef<uint> indices, Inputs &inputs, Outputs &outputs) const = 0;
};

class MultiFunction_AddFloats : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.single_input<float>("A");
    signature.single_input<float>("B");
    signature.single_output<float>("Result");
  }

  void call(ArrayRef<uint> indices, Inputs &inputs, Outputs &outputs) const override
  {
    auto a = inputs.get<float>(0, "A");
    auto b = inputs.get<float>(1, "B");
    auto result = outputs.get<float>(0, "Result");

    for (uint i : indices) {
      result[i] = a[i] + b[i];
    }
  }
};

class MultiFunction_VectorDistance : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.single_input<float3>("A");
    signature.single_input<float3>("A");
    signature.single_output<float>("Distances");
  }

  void call(ArrayRef<uint> indices, Inputs &inputs, Outputs &outputs) const override
  {
    auto a = inputs.get<float3>(0, "A");
    auto b = inputs.get<float3>(1, "B");
    auto distances = outputs.get<float>(0, "Distances");

    for (uint i : indices) {
      distances[i] = float3::distance(a[i], b[i]);
    }
  }
};

}  // namespace BKE