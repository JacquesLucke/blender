#include "BKE_node_functions.h"
#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"

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
  Vector<CPPType *> m_vector_input_types;
  Vector<std::string> m_vector_input_names;

 public:
  class Signature {
   private:
    MultiFunction &m_function;

    Signature(MultiFunction &function) : m_function(function)
    {
    }

    friend MultiFunction;

   public:
    template<typename T> void readonly_single_input(StringRef name)
    {
      m_function.m_single_input_names.append(name);
      m_function.m_single_input_types.append(&GET_TYPE<T>());
    }

    template<typename T> void single_output(StringRef name)
    {
      m_function.m_single_output_names.append(name);
      m_function.m_single_output_types.append(&GET_TYPE<T>());
    }

    template<typename T> void readonly_vector_input(StringRef name)
    {
      m_function.m_vector_input_names.append(name);
      m_function.m_vector_input_types.append(&GET_TYPE<T>());
    }
  };

  class Params {
   public:
    template<typename T> ArrayOrSingleRef<T> readonly_single_input(uint index, StringRef name);
    template<typename T> MutableArrayRef<T> single_output(uint index, StringRef name);
    const GenericVectorArray &readonly_vector_input(uint index, StringRef name);
  };

  virtual void signature(Signature &signature) const = 0;
  virtual void call(ArrayRef<uint> indices, Params &params) const = 0;
};

class MultiFunction_AddFloats : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_single_input<float>("A");
    signature.readonly_single_input<float>("B");
    signature.single_output<float>("Result");
  }

  void call(ArrayRef<uint> indices, Params &params) const override
  {
    auto a = params.readonly_single_input<float>(0, "A");
    auto b = params.readonly_single_input<float>(1, "B");
    auto result = params.single_output<float>(0, "Result");

    for (uint i : indices) {
      result[i] = a[i] + b[i];
    }
  }
};

class MultiFunction_VectorDistance : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_single_input<float3>("A");
    signature.readonly_single_input<float3>("A");
    signature.single_output<float>("Distances");
  }

  void call(ArrayRef<uint> indices, Params &params) const override
  {
    auto a = params.readonly_single_input<float3>(0, "A");
    auto b = params.readonly_single_input<float3>(1, "B");
    auto distances = params.single_output<float>(0, "Distances");

    for (uint i : indices) {
      distances[i] = float3::distance(a[i], b[i]);
    }
  }
};

class MultiFunction_FloatArraySum : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_vector_input<float>("Array");
    signature.single_output<float>("Sum");
  }

  void call(ArrayRef<uint> indices, Params &params) const override
  {
    const GenericVectorArray &arrays = params.readonly_vector_input(0, "Array");
    MutableArrayRef<float> sums = params.single_output<float>(0, "Sum");

    for (uint i : indices) {
      ArrayRef<float> array = arrays.as_ref<float>(i);
      float sum = 0.0f;
      for (float value : array) {
        sum += value;
      }
      sums[i] = sum;
    }
  }
};

}  // namespace BKE