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
 public:
  class Signature {
   private:
    MultiFunction &m_function;

    Signature(MultiFunction &function) : m_function(function)
    {
    }

    friend MultiFunction;

   public:
    template<typename T> void readonly_single_input(StringRef name);
    void readonly_single_input(StringRef name, CPPType &type);
    template<typename T> void single_output(StringRef name);
    void single_output(StringRef name, CPPType &base_type);
    template<typename T> void readonly_vector_input(StringRef name);
    void readonly_vector_input(StringRef name, CPPType &base_type);
    template<typename T> void vector_output(StringRef name);
    void mutable_vector(StringRef name, CPPType &base_type);
  };

  class Params {
   public:
    template<typename T> ArrayOrSingleRef<T> readonly_single_input(uint index, StringRef name);
    GenericArrayOrSingleRef readonly_single_input(uint index, StringRef name);

    template<typename T> MutableArrayRef<T> single_output(uint index, StringRef name);
    GenericMutableArrayRef single_output(uint index, StringRef name);

    template<typename T>
    const GenericVectorArray::TypedRef<T> readonly_vector_input(uint index, StringRef name);
    GenericVectorArrayOrSingleRef readonly_vector_input(uint index, StringRef name);

    template<typename T>
    GenericVectorArray::MutableTypedRef<T> vector_output(uint index, StringRef name);
    GenericVectorArray &vector_output(uint index, StringRef name);

    GenericVectorArray &mutable_vector(uint index, StringRef name);
  };

  virtual void signature(Signature &signature) const = 0;
  virtual void call(ArrayRef<uint> mask_indices, Params &params) const = 0;
};

class MultiFunction_AddFloats final : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_single_input<float>("A");
    signature.readonly_single_input<float>("B");
    signature.single_output<float>("Result");
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    auto a = params.readonly_single_input<float>(0, "A");
    auto b = params.readonly_single_input<float>(1, "B");
    auto result = params.single_output<float>(2, "Result");

    for (uint i : mask_indices) {
      result[i] = a[i] + b[i];
    }
  }
};

class MultiFunction_VectorDistance final : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_single_input<float3>("A");
    signature.readonly_single_input<float3>("A");
    signature.single_output<float>("Distances");
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    auto a = params.readonly_single_input<float3>(0, "A");
    auto b = params.readonly_single_input<float3>(1, "B");
    auto distances = params.single_output<float>(2, "Distances");

    for (uint i : mask_indices) {
      distances[i] = float3::distance(a[i], b[i]);
    }
  }
};

class MultiFunction_FloatArraySum final : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_vector_input<float>("Array");
    signature.single_output<float>("Sum");
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    auto arrays = params.readonly_vector_input<float>(0, "Array");
    MutableArrayRef<float> sums = params.single_output<float>(1, "Sum");

    for (uint i : mask_indices) {
      float sum = 0.0f;
      for (float value : arrays[i]) {
        sum += value;
      }
      sums[i] = sum;
    }
  }
};

class MultiFunction_FloatRange final : public MultiFunction {
  void signature(Signature &signature) const override
  {
    signature.readonly_single_input<float>("Start");
    signature.readonly_single_input<float>("Step");
    signature.readonly_single_input<uint>("Amount");
    signature.vector_output<float>("Range");
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    auto starts = params.readonly_single_input<float>(0, "Start");
    auto steps = params.readonly_single_input<float>(1, "Step");
    auto amounts = params.readonly_single_input<uint>(2, "Amount");
    auto ranges = params.vector_output<float>(3, "Range");

    for (uint i : mask_indices) {
      for (uint j = 0; j < amounts[i]; j++) {
        float value = starts[i] + j * steps[i];
        ranges.append_single(i, value);
      }
    }
  }
};

class MultiFunction_AppendToList final : public MultiFunction {
 private:
  CPPType &m_base_type;

 public:
  void signature(Signature &signature) const override
  {
    signature.mutable_vector("List", m_base_type);
    signature.readonly_single_input("Value", m_base_type);
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    GenericVectorArray &lists = params.mutable_vector(0, "List");
    GenericArrayOrSingleRef values = params.readonly_single_input(1, "Value");

    for (uint i : mask_indices) {
      lists.append_single__copy(i, values[i]);
    }
  }
};

class MultiFunction_GetListElement final : public MultiFunction {
 private:
  CPPType &m_base_type;

 public:
  void signature(Signature &signature) const override
  {
    signature.readonly_vector_input("List", m_base_type);
    signature.readonly_single_input<int>("Index");
    signature.readonly_single_input("Fallback", m_base_type);
    signature.single_output("Value", m_base_type);
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    GenericVectorArrayOrSingleRef lists = params.readonly_vector_input(0, "List");
    ArrayOrSingleRef<int> indices = params.readonly_single_input<int>(1, "Index");
    GenericArrayOrSingleRef fallbacks = params.readonly_single_input(2, "Fallback");

    GenericMutableArrayRef output_values = params.single_output(3, "Value");

    for (uint i : mask_indices) {
      int index = indices[i];
      if (index >= 0) {
        GenericArrayRef list = lists[i];
        if (index < list.size()) {
          m_base_type.copy_to_uninitialized(list[index], output_values[i]);
          continue;
        }
      }
      m_base_type.copy_to_uninitialized(fallbacks[i], output_values[i]);
    }
  }
};

class MultiFunction_ListLength final : public MultiFunction {
 private:
  CPPType &m_base_type;

 public:
  void signature(Signature &signature) const override
  {
    signature.readonly_vector_input("List", m_base_type);
    signature.single_output<int>("Length");
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    GenericVectorArrayOrSingleRef lists = params.readonly_vector_input(0, "List");
    MutableArrayRef<int> lengths = params.single_output<int>(1, "Length");

    for (uint i : mask_indices) {
      lengths[i] = lists[i].size();
    }
  }
};

class MultiFunction_CombineLists final : public MultiFunction {
 private:
  CPPType &m_base_type;

 public:
  void signature(Signature &signature) const override
  {
    signature.mutable_vector("List", m_base_type);
    signature.readonly_vector_input("Other", m_base_type);
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    GenericVectorArray &lists = params.mutable_vector(0, "List");
    GenericVectorArrayOrSingleRef others = params.readonly_vector_input(1, "Other");

    for (uint i : mask_indices) {
      lists.extend_single__copy(i, others[i]);
    }
  }
};

}  // namespace BKE