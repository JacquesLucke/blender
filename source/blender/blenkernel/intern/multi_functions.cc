#include "BKE_generic_array_ref.h"
#include "BKE_generic_vector_array.h"
#include "BKE_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_lazy_init_cxx.h"
#include "BLI_string_map.h"

#include "DNA_object_types.h"

namespace BKE {

using BLI::float3;

MultiFunction_AddFloats::MultiFunction_AddFloats()
{
  MFSignatureBuilder signature("Add Floats");
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
  MFSignatureBuilder signature("Add Float3s");
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
  MFSignatureBuilder signature("Combine Vector");
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
  MFSignatureBuilder signature("Separate Vector");
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
  MFSignatureBuilder signature("Vector Distance");
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
  MFSignatureBuilder signature("Float Array Sum");
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
  MFSignatureBuilder signature("Float Range");
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

MultiFunction_ObjectWorldLocation::MultiFunction_ObjectWorldLocation()
{
  MFSignatureBuilder signature("Object Location");
  signature.readonly_single_input<Object *>("Object");
  signature.single_output<float3>("Location");
  this->set_signature(signature);
}

void MultiFunction_ObjectWorldLocation::call(ArrayRef<uint> mask_indices,
                                             MFParams &params,
                                             MFContext &UNUSED(context)) const
{
  auto objects = params.readonly_single_input<Object *>(0, "Object");
  auto locations = params.single_output<float3>(1, "Location");

  for (uint i : mask_indices) {
    if (objects[i] != nullptr) {
      locations[i] = objects[i]->obmat[3];
    }
    else {
      locations[i] = float3(0, 0, 0);
    }
  }
}

MultiFunction_PackList::MultiFunction_PackList(const CPPType &base_type,
                                               ArrayRef<bool> input_list_status)
    : m_base_type(base_type), m_input_list_status(input_list_status)
{
  MFSignatureBuilder signature("Pack List");
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
  MFSignatureBuilder signature("Get List Element");
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
  MFSignatureBuilder signature("List Length");
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

MultiFunction_SimpleVectorize::MultiFunction_SimpleVectorize(const MultiFunction &function,
                                                             ArrayRef<bool> input_is_vectorized)
    : m_function(function), m_input_is_vectorized(input_is_vectorized)
{
  BLI_assert(input_is_vectorized.contains(true));

  MFSignatureBuilder signature(function.name() + " (Vectorized)");

  bool found_output_param = false;
  UNUSED_VARS_NDEBUG(found_output_param);
  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    switch (param_type.category()) {
      case MFParamType::None:
      case MFParamType::ReadonlyVectorInput:
      case MFParamType::VectorOutput:
      case MFParamType::MutableVector: {
        BLI_assert(false);
        break;
      }
      case MFParamType::ReadonlySingleInput: {
        BLI_assert(!found_output_param);
        if (input_is_vectorized[param_index]) {
          signature.readonly_vector_input("Input", param_type.type());
          m_vectorized_inputs.append(param_index);
        }
        else {
          signature.readonly_single_input("Input", param_type.type());
        }
        break;
      }
      case MFParamType::SingleOutput: {
        signature.vector_output("Output", param_type.type());
        m_output_indices.append(param_index);
        found_output_param = true;
        break;
      }
    }
  }
  this->set_signature(signature);
}

void MultiFunction_SimpleVectorize::call(ArrayRef<uint> mask_indices,
                                         MFParams &params,
                                         MFContext &context) const
{
  if (mask_indices.size() == 0) {
    return;
  }
  uint array_size = mask_indices.last() + 1;

  Vector<int> vectorization_lengths(array_size);
  vectorization_lengths.fill_indices(mask_indices, -1);

  for (uint param_index : m_vectorized_inputs) {
    GenericVirtualListListRef values = params.readonly_vector_input(param_index, "Input");
    for (uint i : mask_indices) {
      if (vectorization_lengths[i] != 0) {
        vectorization_lengths[i] = std::max<int>(vectorization_lengths[i], values[i].size());
      }
    }
  }

  Vector<GenericVectorArray *> output_vector_arrays;
  for (uint param_index : m_output_indices) {
    GenericVectorArray *vector_array = &params.vector_output(param_index, "Output");
    output_vector_arrays.append(vector_array);
  }

  for (uint index : mask_indices) {
    uint length = vectorization_lengths[index];
    MFParamsBuilder params_builder(m_function, length);

    for (uint param_index : m_function.param_indices()) {
      MFParamType param_type = m_function.param_type(param_index);
      switch (param_type.category()) {
        case MFParamType::None:
        case MFParamType::ReadonlyVectorInput:
        case MFParamType::VectorOutput:
        case MFParamType::MutableVector: {
          BLI_assert(false);
          break;
        }
        case MFParamType::ReadonlySingleInput: {
          if (m_input_is_vectorized[param_index]) {
            GenericVirtualListListRef input_list_list = params.readonly_vector_input(param_index,
                                                                                     "Input");
            GenericVirtualListRef repeated_input = input_list_list.repeated_sublist(index, length);
            params_builder.add_readonly_single_input(repeated_input);
          }
          else {
            GenericVirtualListRef input_list = params.readonly_single_input(param_index, "Input");
            GenericVirtualListRef repeated_input = input_list.repeated_element(index, length);
            params_builder.add_readonly_single_input(repeated_input);
          }
          break;
        }
        case MFParamType::SingleOutput: {
          GenericVectorArray &output_array_list = params.vector_output(param_index, "Output");
          GenericMutableArrayRef output_array = output_array_list.allocate_single(index, length);
          params_builder.add_single_output(output_array);
          break;
        }
      }
    }

    ArrayRef<uint> sub_mask_indices = IndexRange(length).as_array_ref();
    m_function.call(sub_mask_indices, params_builder.build(), context);
  }
}

}  // namespace BKE
