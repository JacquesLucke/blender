#pragma once

#include "FN_multi_function_network.h"

#include "BLI_map.h"
#include "BLI_stack_cxx.h"

namespace FN {

using BLI::Map;
using BLI::Stack;

class MF_EvaluateNetwork_Storage;

class MF_EvaluateNetwork final : public MultiFunction {
 private:
  Vector<const MFOutputSocket *> m_inputs;
  Vector<const MFInputSocket *> m_outputs;

 public:
  MF_EvaluateNetwork(Vector<const MFOutputSocket *> inputs, Vector<const MFInputSocket *> outputs);

  void call(MFMask mask, MFParams params, MFContext context) const override;

 private:
  using Storage = MF_EvaluateNetwork_Storage;

  void copy_inputs_to_storage(MFParams params, Storage &storage) const;
  void copy_inputs_to_storage__single(GenericVirtualListRef input_list,
                                      ArrayRef<const MFInputSocket *> targets,
                                      Storage &storage) const;
  void copy_inputs_to_storage__vector(GenericVirtualListListRef input_list_list,
                                      ArrayRef<const MFInputSocket *> targets,
                                      Storage &storage) const;

  void evaluate_network_to_compute_outputs(MFContext &global_context, Storage &storage) const;

  void compute_and_forward_outputs(MFContext &global_context,
                                   const MFFunctionNode &function_node,
                                   Storage &storage) const;
  void prepare_function_params(const MFFunctionNode &function_node,
                               Storage &storage,
                               MFParamsBuilder &params_builder) const;
  void forward_computed_values(const MFFunctionNode &function_node,
                               Storage &storage,
                               MFParamsBuilder &params_builder) const;

  void copy_computed_values_to_outputs(MFParams params, Storage &storage) const;
};

}  // namespace FN
