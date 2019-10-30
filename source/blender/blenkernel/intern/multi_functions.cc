#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"
#include "BKE_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_lazy_init_cxx.h"
#include "BLI_string_map.h"

namespace BKE {

using BLI::float3;

MultiFunction_AddFloats::MultiFunction_AddFloats()
{
  MFSignatureBuilder signature;
  signature.readonly_single_input<float>("A");
  signature.readonly_single_input<float>("B");
  signature.single_output<float>("Result");
  this->set_signature(signature);
}

void MultiFunction_AddFloats::call(ArrayRef<uint> mask_indices,
                                   MFParams &params,
                                   MFContext &UNUSED(context)) const
{
  auto a = params.readonly_single_input<float>(0, "A");
  auto b = params.readonly_single_input<float>(1, "B");
  auto result = params.single_output<float>(2, "Result");

  for (uint i : mask_indices) {
    result[i] = a[i] + b[i];
  }
}

MultiFunction_AddFloat3s::MultiFunction_AddFloat3s()
{
  MFSignatureBuilder signature;
  signature.readonly_single_input<float3>("A");
  signature.readonly_single_input<float3>("B");
  signature.single_output<float3>("Result");
  this->set_signature(signature);
}

void MultiFunction_AddFloat3s::call(ArrayRef<uint> mask_indices,
                                    MFParams &params,
                                    MFContext &UNUSED(context)) const
{
  auto a = params.readonly_single_input<float3>(0, "A");
  auto b = params.readonly_single_input<float3>(1, "B");
  auto result = params.single_output<float3>(2, "Result");

  for (uint i : mask_indices) {
    result[i] = a[i] + b[i];
  }
}

MultiFunction_CombineVector::MultiFunction_CombineVector()
{
  MFSignatureBuilder signature;
  signature.readonly_single_input<float>("X");
  signature.readonly_single_input<float>("Y");
  signature.readonly_single_input<float>("Z");
  signature.single_output<float3>("Vector");
  this->set_signature(signature);
}

void MultiFunction_CombineVector::call(ArrayRef<uint> mask_indices,
                                       MFParams &params,
                                       MFContext &UNUSED(context)) const
{
  auto x = params.readonly_single_input<float>(0, "X");
  auto y = params.readonly_single_input<float>(1, "Y");
  auto z = params.readonly_single_input<float>(2, "Z");
  auto vector = params.single_output<float3>(3, "Vector");

  for (uint i : mask_indices) {
    vector[i] = {x[i], y[i], z[i]};
  }
}

MultiFunction_SeparateVector::MultiFunction_SeparateVector()
{
  MFSignatureBuilder signature;
  signature.readonly_single_input<float3>("Vector");
  signature.single_output<float>("X");
  signature.single_output<float>("Y");
  signature.single_output<float>("Z");
  this->set_signature(signature);
}

void MultiFunction_SeparateVector::call(ArrayRef<uint> mask_indices,
                                        MFParams &params,
                                        MFContext &UNUSED(context)) const
{
  auto vector = params.readonly_single_input<float3>(0, "Vector");
  auto x = params.single_output<float>(1, "X");
  auto y = params.single_output<float>(2, "Y");
  auto z = params.single_output<float>(3, "Z");

  for (uint i : mask_indices) {
    float3 v = vector[i];
    x[i] = v.x;
    y[i] = v.y;
    z[i] = v.z;
  }
}

MultiFunction_VectorDistance::MultiFunction_VectorDistance()
{
  MFSignatureBuilder signature;
  signature.readonly_single_input<float3>("A");
  signature.readonly_single_input<float3>("A");
  signature.single_output<float>("Distances");
  this->set_signature(signature);
}

void MultiFunction_VectorDistance::call(ArrayRef<uint> mask_indices,
                                        MFParams &params,
                                        MFContext &UNUSED(context)) const
{
  auto a = params.readonly_single_input<float3>(0, "A");
  auto b = params.readonly_single_input<float3>(1, "B");
  auto distances = params.single_output<float>(2, "Distances");

  for (uint i : mask_indices) {
    distances[i] = float3::distance(a[i], b[i]);
  }
}

MultiFunction_FloatArraySum::MultiFunction_FloatArraySum()
{
  MFSignatureBuilder signature;
  signature.readonly_vector_input<float>("Array");
  signature.single_output<float>("Sum");
  this->set_signature(signature);
}

void MultiFunction_FloatArraySum::call(ArrayRef<uint> mask_indices,
                                       MFParams &params,
                                       MFContext &UNUSED(context)) const
{
  auto arrays = params.readonly_vector_input<float>(0, "Array");
  MutableArrayRef<float> sums = params.single_output<float>(1, "Sum");

  for (uint i : mask_indices) {
    float sum = 0.0f;
    VirtualListRef<float> array = arrays[i];
    for (uint j = 0; j < array.size(); j++) {
      sum += array[j];
    }
    sums[i] = sum;
  }
}

MultiFunction_FloatRange::MultiFunction_FloatRange()
{
  MFSignatureBuilder signature;
  signature.readonly_single_input<float>("Start");
  signature.readonly_single_input<float>("Step");
  signature.readonly_single_input<int>("Amount");
  signature.vector_output<float>("Range");
  this->set_signature(signature);
}

void MultiFunction_FloatRange::call(ArrayRef<uint> mask_indices,
                                    MFParams &params,
                                    MFContext &UNUSED(context)) const
{
  auto starts = params.readonly_single_input<float>(0, "Start");
  auto steps = params.readonly_single_input<float>(1, "Step");
  auto amounts = params.readonly_single_input<int>(2, "Amount");
  auto ranges = params.vector_output<float>(3, "Range");

  for (uint i : mask_indices) {
    for (uint j = 0; j < amounts[i]; j++) {
      float value = starts[i] + j * steps[i];
      ranges.append_single(i, value);
    }
  }
}

MultiFunction_AppendToList::MultiFunction_AppendToList(const CPPType &base_type)
    : m_base_type(base_type)
{
  MFSignatureBuilder signature;
  signature.mutable_vector("List", m_base_type);
  signature.readonly_single_input("Value", m_base_type);
  this->set_signature(signature);
}

void MultiFunction_AppendToList::call(ArrayRef<uint> mask_indices,
                                      MFParams &params,
                                      MFContext &UNUSED(context)) const
{
  GenericVectorArray &lists = params.mutable_vector(0, "List");
  GenericVirtualListRef values = params.readonly_single_input(1, "Value");

  for (uint i : mask_indices) {
    lists.append_single__copy(i, values[i]);
  }
}

MultiFunction_PackList::MultiFunction_PackList(const CPPType &base_type,
                                               ArrayRef<bool> input_list_status)
    : m_base_type(base_type), m_input_list_status(input_list_status)
{
  MFSignatureBuilder signature;
  if (m_input_list_status.size() == 0) {
    /* Output just an empty list. */
    signature.vector_output("List", m_base_type);
  }
  else if (this->input_is_list(0)) {
    /* Extend the first incoming list. */
    signature.mutable_vector("List", m_base_type);
    for (uint i = 1; i < m_input_list_status.size(); i++) {
      if (this->input_is_list(i)) {
        signature.readonly_vector_input("List", m_base_type);
      }
      else {
        signature.readonly_single_input("Value", m_base_type);
      }
    }
  }
  else {
    /* Create a new list and append everything. */
    for (uint i = 0; i < m_input_list_status.size(); i++) {
      if (this->input_is_list(i)) {
        signature.readonly_vector_input("List", m_base_type);
      }
      else {
        signature.readonly_single_input("Value", m_base_type);
      }
    }
    signature.vector_output("List", m_base_type);
  }
  this->set_signature(signature);
}

void MultiFunction_PackList::call(ArrayRef<uint> mask_indices,
                                  MFParams &params,
                                  MFContext &UNUSED(context)) const
{
  GenericVectorArray *vector_array;
  bool is_mutating_first_list;
  if (m_input_list_status.size() == 0) {
    vector_array = &params.vector_output(0, "List");
    is_mutating_first_list = false;
  }
  else if (this->input_is_list(0)) {
    vector_array = &params.mutable_vector(0, "List");
    is_mutating_first_list = true;
  }
  else {
    vector_array = &params.vector_output(m_input_list_status.size(), "List");
    is_mutating_first_list = false;
  }

  uint first_index = is_mutating_first_list ? 1 : 0;
  for (uint input_index = first_index; input_index < m_input_list_status.size(); input_index++) {
    if (this->input_is_list(input_index)) {
      GenericVirtualListListRef list = params.readonly_vector_input(input_index, "List");
      for (uint i : mask_indices) {
        vector_array->extend_single__copy(i, list[i]);
      }
    }
    else {
      GenericVirtualListRef list = params.readonly_single_input(input_index, "Value");
      for (uint i : mask_indices) {
        vector_array->append_single__copy(i, list[i]);
      }
    }
  }
}

bool MultiFunction_PackList::input_is_list(uint index) const
{
  return m_input_list_status[index];
}

MultiFunction_GetListElement::MultiFunction_GetListElement(const CPPType &base_type)
    : m_base_type(base_type)
{
  MFSignatureBuilder signature;
  signature.readonly_vector_input("List", m_base_type);
  signature.readonly_single_input<int>("Index");
  signature.readonly_single_input("Fallback", m_base_type);
  signature.single_output("Value", m_base_type);
  this->set_signature(signature);
}

void MultiFunction_GetListElement::call(ArrayRef<uint> mask_indices,
                                        MFParams &params,
                                        MFContext &UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  VirtualListRef<int> indices = params.readonly_single_input<int>(1, "Index");
  GenericVirtualListRef fallbacks = params.readonly_single_input(2, "Fallback");

  GenericMutableArrayRef output_values = params.single_output(3, "Value");

  for (uint i : mask_indices) {
    int index = indices[i];
    if (index >= 0) {
      GenericVirtualListRef list = lists[i];
      if (index < list.size()) {
        m_base_type.copy_to_uninitialized(list[index], output_values[i]);
        continue;
      }
    }
    m_base_type.copy_to_uninitialized(fallbacks[i], output_values[i]);
  }
}

MultiFunction_ListLength::MultiFunction_ListLength(const CPPType &base_type)
    : m_base_type(base_type)
{
  MFSignatureBuilder signature;
  signature.readonly_vector_input("List", m_base_type);
  signature.single_output<int>("Length");
  this->set_signature(signature);
}

void MultiFunction_ListLength::call(ArrayRef<uint> mask_indices,
                                    MFParams &params,
                                    MFContext &UNUSED(context)) const
{
  GenericVirtualListListRef lists = params.readonly_vector_input(0, "List");
  MutableArrayRef<int> lengths = params.single_output<int>(1, "Length");

  for (uint i : mask_indices) {
    lengths[i] = lists[i].size();
  }
}

MultiFunction_CombineLists::MultiFunction_CombineLists(const CPPType &base_type)
    : m_base_type(base_type)
{
  MFSignatureBuilder signature;
  signature.mutable_vector("List", m_base_type);
  signature.readonly_vector_input("Other", m_base_type);
  this->set_signature(signature);
}

void MultiFunction_CombineLists::call(ArrayRef<uint> mask_indices,
                                      MFParams &params,
                                      MFContext &UNUSED(context)) const
{
  GenericVectorArray &lists = params.mutable_vector(0, "List");
  GenericVirtualListListRef others = params.readonly_vector_input(1, "Other");

  for (uint i : mask_indices) {
    lists.extend_single__copy(i, others[i]);
  }
}

}  // namespace BKE
