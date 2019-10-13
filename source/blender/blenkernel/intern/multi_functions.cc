#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"
#include "BKE_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_lazy_init_cxx.h"
#include "BLI_string_map.h"
#include "BLI_array_or_single_ref.h"

namespace BKE {

using BLI::ArrayOrSingleRef;
using BLI::float3;

void MultiFunction_AddFloats::signature(SignatureBuilder &signature) const
{
  signature.readonly_single_input<float>("A");
  signature.readonly_single_input<float>("B");
  signature.single_output<float>("Result");
}

void MultiFunction_AddFloats::call(ArrayRef<uint> mask_indices, Params &params) const
{
  auto a = params.readonly_single_input<float>(0, "A");
  auto b = params.readonly_single_input<float>(1, "B");
  auto result = params.single_output<float>(2, "Result");

  for (uint i : mask_indices) {
    result[i] = a[i] + b[i];
  }
}

void MultiFunction_VectorDistance::signature(SignatureBuilder &signature) const
{
  signature.readonly_single_input<float3>("A");
  signature.readonly_single_input<float3>("A");
  signature.single_output<float>("Distances");
}

void MultiFunction_VectorDistance::call(ArrayRef<uint> mask_indices, Params &params) const
{
  auto a = params.readonly_single_input<float3>(0, "A");
  auto b = params.readonly_single_input<float3>(1, "B");
  auto distances = params.single_output<float>(2, "Distances");

  for (uint i : mask_indices) {
    distances[i] = float3::distance(a[i], b[i]);
  }
}

void MultiFunction_FloatArraySum::signature(SignatureBuilder &signature) const
{
  signature.readonly_vector_input<float>("Array");
  signature.single_output<float>("Sum");
}

void MultiFunction_FloatArraySum::call(ArrayRef<uint> mask_indices, Params &params) const
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

void MultiFunction_FloatRange::signature(SignatureBuilder &signature) const
{
  signature.readonly_single_input<float>("Start");
  signature.readonly_single_input<float>("Step");
  signature.readonly_single_input<uint>("Amount");
  signature.vector_output<float>("Range");
}

void MultiFunction_FloatRange::call(ArrayRef<uint> mask_indices, Params &params) const
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

void MultiFunction_AppendToList::signature(SignatureBuilder &signature) const
{
  signature.mutable_vector("List", m_base_type);
  signature.readonly_single_input("Value", m_base_type);
}

void MultiFunction_AppendToList::call(ArrayRef<uint> mask_indices, Params &params) const
{
  GenericVectorArray &lists = params.mutable_vector(0, "List");
  GenericArrayOrSingleRef values = params.readonly_single_input(1, "Value");

  for (uint i : mask_indices) {
    lists.append_single__copy(i, values[i]);
  }
}

void MultiFunction_GetListElement::signature(SignatureBuilder &signature) const
{
  signature.readonly_vector_input("List", m_base_type);
  signature.readonly_single_input<int>("Index");
  signature.readonly_single_input("Fallback", m_base_type);
  signature.single_output("Value", m_base_type);
}

void MultiFunction_GetListElement::call(ArrayRef<uint> mask_indices, Params &params) const
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

void MultiFunction_ListLength::signature(SignatureBuilder &signature) const
{
  signature.readonly_vector_input("List", m_base_type);
  signature.single_output<int>("Length");
}

void MultiFunction_ListLength::call(ArrayRef<uint> mask_indices, Params &params) const
{
  GenericVectorArrayOrSingleRef lists = params.readonly_vector_input(0, "List");
  MutableArrayRef<int> lengths = params.single_output<int>(1, "Length");

  for (uint i : mask_indices) {
    lengths[i] = lists[i].size();
  }
}

void MultiFunction_CombineLists::signature(SignatureBuilder &signature) const
{
  signature.mutable_vector("List", m_base_type);
  signature.readonly_vector_input("Other", m_base_type);
}

void MultiFunction_CombineLists::call(ArrayRef<uint> mask_indices, Params &params) const
{
  GenericVectorArray &lists = params.mutable_vector(0, "List");
  GenericVectorArrayOrSingleRef others = params.readonly_vector_input(1, "Other");

  for (uint i : mask_indices) {
    lists.extend_single__copy(i, others[i]);
  }
}

}  // namespace BKE