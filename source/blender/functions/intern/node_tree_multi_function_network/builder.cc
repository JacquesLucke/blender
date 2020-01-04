#include "builder.h"

namespace FN {
namespace MFGeneration {

using BLI::ScopedVector;

MFBuilderFunctionNode &FunctionTreeMFBuilderBase::add_function(const MultiFunction &function)
{
  return m_common.network_builder.add_function(function);
}

MFBuilderFunctionNode &FunctionTreeMFBuilderBase::add_function(const MultiFunction &function,
                                                               const FNode &fnode)
{
  MFBuilderFunctionNode &node = m_common.network_builder.add_function(function);
  m_common.socket_map.add(fnode, node, m_common.fsocket_data_types);
  return node;
}

MFBuilderDummyNode &FunctionTreeMFBuilderBase::add_dummy(const FNode &fnode)
{
  ScopedVector<MFDataType> input_types;
  ScopedVector<StringRef> input_names;
  for (const FInputSocket *fsocket : fnode.inputs()) {
    Optional<MFDataType> data_type = m_common.fsocket_data_types.try_lookup_data_type(*fsocket);
    if (data_type.has_value()) {
      input_types.append(data_type.value());
      input_names.append(fsocket->name());
    }
  }

  ScopedVector<MFDataType> output_types;
  ScopedVector<StringRef> output_names;
  for (const FOutputSocket *fsocket : fnode.outputs()) {
    Optional<MFDataType> data_type = m_common.fsocket_data_types.try_lookup_data_type(*fsocket);
    if (data_type.has_value()) {
      output_types.append(data_type.value());
      output_names.append(fsocket->name());
    }
  }

  MFBuilderDummyNode &node = m_common.network_builder.add_dummy(
      fnode.name(), input_types, output_types, input_names, output_names);
  m_common.socket_map.add(fnode, node, m_common.fsocket_data_types);
  return node;
}

Vector<bool> FNodeMFNetworkBuilder::get_list_base_variadic_states(StringRefNull prop_name)
{
  Vector<bool> states;
  RNA_BEGIN (m_fnode.rna(), itemptr, prop_name.data()) {
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      states.append(false);
    }
    else if (state == 1) {
      /* list case */
      states.append(true);
    }
    else {
      BLI_assert(false);
    }
  }
  RNA_END;
  return states;
}

void FNodeMFNetworkBuilder::set_matching_fn(const MultiFunction &fn)
{
  MFBuilderFunctionNode &node = this->add_function(fn);
  m_common.socket_map.add(m_fnode, node, m_common.fsocket_data_types);
}

const MultiFunction &FNodeMFNetworkBuilder::get_vectorized_function(
    const MultiFunction &base_function, ArrayRef<const char *> is_vectorized_prop_names)
{
  ScopedVector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(m_fnode.rna(), prop_name, state);
    BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

    bool is_vectorized = STREQ(state, "LIST");
    input_is_vectorized.append(is_vectorized);
  }

  if (input_is_vectorized.contains(true)) {
    return this->construct_fn<MF_SimpleVectorize>(base_function, input_is_vectorized);
  }
  else {
    return base_function;
  }
}

}  // namespace MFGeneration
}  // namespace FN
