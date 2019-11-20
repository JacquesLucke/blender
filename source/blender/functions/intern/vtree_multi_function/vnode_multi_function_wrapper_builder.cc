
#include "vnode_multi_function_wrapper_builder.h"

#include "FN_multi_functions.h"

namespace FN {

const MultiFunction &VNodeMFWrapperBuilder::get_vectorized_function(
    const MultiFunction &base_function, ArrayRef<const char *> is_vectorized_prop_names)
{
  Vector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(m_vnode_to_wrap.rna(), prop_name, state);
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

Vector<bool> VNodeMFWrapperBuilder::get_list_base_variadic_states(StringRefNull prop_name)
{
  Vector<bool> states;
  RNA_BEGIN (m_vnode_to_wrap.rna(), itemptr, prop_name.data()) {
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

void VNodeMFWrapperBuilder::set_matching_fn(const MultiFunction &fn)
{
  Vector<VSocketsForMFParam> param_vsockets;

  Vector<const VInputSocket *> data_input_sockets;
  Vector<const VOutputSocket *> data_output_sockets;
  for (const VInputSocket *vsocket : m_vnode_to_wrap.inputs()) {
    if (m_globals.vsocket_data_types.is_data_socket(*vsocket)) {
      data_input_sockets.append(vsocket);
    }
  }
  for (const VOutputSocket *vsocket : m_vnode_to_wrap.outputs()) {
    if (m_globals.vsocket_data_types.is_data_socket(*vsocket)) {
      data_output_sockets.append(vsocket);
    }
  }

  uint input_index = 0;
  uint output_index = 0;
  for (uint param_index : fn.param_indices()) {
    MFParamType param_type = fn.param_type(param_index);
    switch (param_type.interface_type()) {
      case MFParamType::InterfaceType::Input: {
        param_vsockets.append({data_input_sockets[input_index], nullptr});
        input_index++;
        break;
      }
      case MFParamType::InterfaceType::Output: {
        param_vsockets.append({nullptr, data_output_sockets[output_index]});
        output_index++;
        break;
      }
      case MFParamType::InterfaceType::Mutable: {
        param_vsockets.append(
            {data_input_sockets[input_index], data_output_sockets[output_index]});
        input_index++;
        output_index++;
        break;
      }
    }
  }

  this->set_fn(fn, std::move(param_vsockets));
}

void VNodeMFWrapperBuilder::set_fn(const MultiFunction &fn,
                                   Vector<VSocketsForMFParam> param_vsockets)
{
  this->assert_valid_param_vsockets(fn, param_vsockets);

  m_wrapper_to_build.function = &fn;
  m_wrapper_to_build.param_vsockets = std::move(param_vsockets);
}

void VNodeMFWrapperBuilder::assert_valid_param_vsockets(
    const MultiFunction &fn, ArrayRef<VSocketsForMFParam> param_vsockets)
{
#ifdef DEBUG
  BLI_assert(fn.param_indices().size() == param_vsockets.size());

  /* Check that the correct vsockets are initialized. */
  for (uint param_index : fn.param_indices()) {
    MFParamType param_type = fn.param_type(param_index);
    const VSocketsForMFParam &vsockets = param_vsockets[param_index];
    switch (param_type.interface_type()) {
      case MFParamType::Input:
        BLI_assert(vsockets.input_vsocket != nullptr);
        BLI_assert(vsockets.output_vsocket == nullptr);
        break;
      case MFParamType::Output:
        BLI_assert(vsockets.input_vsocket == nullptr);
        BLI_assert(vsockets.output_vsocket != nullptr);
        break;
      case MFParamType::Mutable:
        BLI_assert(vsockets.input_vsocket == nullptr);
        break;
    }
    MFDataType expected_data_type = param_type.data_type();
    if (vsockets.input_vsocket != nullptr) {
      BLI_assert(expected_data_type ==
                 m_globals.vsocket_data_types.lookup(*vsockets.input_vsocket));
    }
    if (vsockets.output_vsocket != nullptr) {
      BLI_assert(expected_data_type ==
                 m_globals.vsocket_data_types.lookup(*vsockets.output_vsocket));
    }
  }

  /* Check that no vsocket is used twice. */
  Vector<const VInputSocket *> input_vsockets;
  Vector<const VOutputSocket *> output_vsockets;
  for (const VSocketsForMFParam &vsocket : param_vsockets) {
    if (vsocket.input_vsocket != nullptr) {
      input_vsockets.append(vsocket.input_vsocket);
    }
    if (vsocket.output_vsocket != nullptr) {
      output_vsockets.append(vsocket.output_vsocket);
    }
  }
  BLI_assert(!ArrayRef<const VInputSocket *>(input_vsockets).has_duplicates__linear_search());
  BLI_assert(!ArrayRef<const VOutputSocket *>(output_vsockets).has_duplicates__linear_search());

  /* Check that all required outputs are computed. */
  for (const VOutputSocket *required_output : m_outputs_to_compute) {
    BLI_assert(output_vsockets.contains(required_output));
  }
#endif
}

}  // namespace FN
