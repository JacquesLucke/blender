#include "vectorize.h"

#include "BLI_array_cxx.h"

namespace FN {

using BLI::Array;

MF_SimpleVectorize::MF_SimpleVectorize(const MultiFunction &function,
                                       ArrayRef<bool> input_is_vectorized)
    : m_function(function), m_input_is_vectorized(input_is_vectorized)
{
  BLI_assert(input_is_vectorized.contains(true));

  MFSignatureBuilder signature = this->get_builder(function.name() + " (Vectorized)");
  signature.copy_used_contexts(function);

  bool found_output_param = false;
  UNUSED_VARS_NDEBUG(found_output_param);

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    MFDataType data_type = param_type.data_type();
    StringRef param_name = function.param_name(param_index);

    switch (param_type.type()) {
      case MFParamType::VectorInput:
      case MFParamType::VectorOutput:
      case MFParamType::MutableVector:
      case MFParamType::MutableSingle: {
        BLI_assert(false);
        break;
      }
      case MFParamType::SingleInput: {
        BLI_assert(!found_output_param);
        if (input_is_vectorized[param_index]) {
          signature.vector_input(param_name + " (List)", data_type.single__cpp_type());
          m_vectorized_inputs.append(param_index);
        }
        else {
          signature.single_input(param_name, data_type.single__cpp_type());
        }
        break;
      }
      case MFParamType::SingleOutput: {
        signature.vector_output(param_name + " (List)", data_type.single__cpp_type());
        m_output_indices.append(param_index);
        found_output_param = true;
        break;
      }
    }
  }
}

static void get_vectorization_lengths(IndexMask mask,
                                      MFParams params,
                                      ArrayRef<uint> vectorized_param_indices,
                                      MutableArrayRef<int> r_lengths)
{
  r_lengths.fill_indices(mask.indices(), -1);

  for (uint param_index : vectorized_param_indices) {
    GenericVirtualListListRef values = params.readonly_vector_input(param_index);
    for (uint i : mask.indices()) {
      if (r_lengths[i] != 0) {
        uint sublist_size = values.sublist_size(i);
        r_lengths[i] = std::max<int>(r_lengths[i], sublist_size);
      }
    }
  }
}

void MF_SimpleVectorize::call(IndexMask mask, MFParams params, MFContext context) const
{
  if (mask.size() == 0) {
    return;
  }

  Array<int> vectorization_lengths(mask.min_array_size());
  get_vectorization_lengths(mask, params, m_vectorized_inputs, vectorization_lengths);

  MFContextBuilder sub_context_builder;
  sub_context_builder.add_global_contexts(context);

  for (uint index : mask.indices()) {
    uint length = vectorization_lengths[index];
    MFParamsBuilder params_builder(m_function, length);

    for (uint param_index : m_function.param_indices()) {
      MFParamType param_type = m_function.param_type(param_index);
      switch (param_type.type()) {
        case MFParamType::VectorInput:
        case MFParamType::VectorOutput:
        case MFParamType::MutableVector:
        case MFParamType::MutableSingle: {
          BLI_assert(false);
          break;
        }
        case MFParamType::SingleInput: {
          if (m_input_is_vectorized[param_index]) {
            GenericVirtualListListRef input_list_list = params.readonly_vector_input(param_index);
            GenericVirtualListRef repeated_input = input_list_list.repeated_sublist(index, length);
            params_builder.add_readonly_single_input(repeated_input);
          }
          else {
            GenericVirtualListRef input_list = params.readonly_single_input(param_index);
            GenericVirtualListRef repeated_input = input_list.repeated_element(index, length);
            params_builder.add_readonly_single_input(repeated_input);
          }
          break;
        }
        case MFParamType::SingleOutput: {
          GenericVectorArray &output_array_list = params.vector_output(param_index);
          GenericMutableArrayRef output_array = output_array_list.allocate_single(index, length);
          params_builder.add_single_output(output_array);
          break;
        }
      }
    }

    /* TODO: Pass modified per element contexts. */
    ArrayRef<uint> sub_mask_indices = IndexRange(length).as_array_ref();
    m_function.call(sub_mask_indices, params_builder, sub_context_builder);
  }
}

}  // namespace FN
