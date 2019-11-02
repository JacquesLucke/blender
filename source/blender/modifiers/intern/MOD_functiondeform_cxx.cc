#include "DNA_modifier_types.h"

#include "FN_vtree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"

#include "DEG_depsgraph_query.h"

using BKE::VirtualNodeTree;
using BKE::VNode;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::TemporaryVector;
using BLI::Vector;
using FN::MFContext;
using FN::MFInputSocket;
using FN::MFOutputSocket;
using FN::MFParamsBuilder;

extern "C" {
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts);
}

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts)
{
  if (fdmd->function_tree == nullptr) {
    return;
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fdmd->function_tree);

  BKE::VirtualNodeTreeBuilder vtree_builder;
  vtree_builder.add_all_of_node_tree(btree);
  auto vtree = vtree_builder.build();

  const VNode &input_vnode = *vtree->nodes_with_idname("fn_FunctionInputNode")[0];
  const VNode &output_vnode = *vtree->nodes_with_idname("fn_FunctionOutputNode")[0];

  BLI::OwnedResources resources;
  auto vtree_network = FN::generate_vtree_multi_function_network(*vtree, resources);

  Vector<const MFOutputSocket *> function_inputs = {
      &vtree_network->lookup_socket(input_vnode.output(0)).as_output(),
      &vtree_network->lookup_socket(input_vnode.output(1)).as_output(),
      &vtree_network->lookup_socket(input_vnode.output(2)).as_output()};

  Vector<const MFInputSocket *> function_outputs = {
      &vtree_network->lookup_socket(output_vnode.input(0)).as_input()};

  FN::MF_EvaluateNetwork function{function_inputs, function_outputs};

  MFParamsBuilder params(function, numVerts);
  params.add_readonly_single_input(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params.add_readonly_single_input(&fdmd->control1);
  params.add_readonly_single_input(&fdmd->control2);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_single_output<float3>(output_vectors);

  MFContext context;
  function.call(IndexRange(numVerts).as_array_ref(), params.build(), context);

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
