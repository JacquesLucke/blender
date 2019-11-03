#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_mesh.h"

#include "BLI_math.h"

#include "FN_vtree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

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
Mesh *MOD_functionpoints_do(FunctionPointsModifierData *fpmd);
}

Mesh *MOD_functionpoints_do(FunctionPointsModifierData *fpmd)
{
  if (fpmd->function_tree == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

  BKE::VirtualNodeTreeBuilder vtree_builder;
  vtree_builder.add_all_of_node_tree(btree);
  auto vtree = vtree_builder.build();

  const VNode &input_vnode = *vtree->nodes_with_idname("fn_FunctionInputNode")[0];
  const VNode &output_vnode = *vtree->nodes_with_idname("fn_FunctionOutputNode")[0];

  BLI::OwnedResources resources;
  auto vtree_network = FN::generate_vtree_multi_function_network(*vtree, resources);

  Vector<const MFOutputSocket *> function_inputs = {
      &vtree_network->lookup_socket(input_vnode.output(0)).as_output(),
      &vtree_network->lookup_socket(input_vnode.output(1)).as_output()};

  Vector<const MFInputSocket *> function_outputs = {
      &vtree_network->lookup_socket(output_vnode.input(0)).as_input()};

  FN::MF_EvaluateNetwork function{function_inputs, function_outputs};

  MFParamsBuilder params(function, 1);
  params.add_readonly_single_input(&fpmd->control1);
  params.add_readonly_single_input(&fpmd->control2);

  FN::GenericVectorArray vector_array{FN::GET_TYPE<float3>(), 1};
  params.add_vector_output(vector_array);

  MFContext context;
  function.call(FN::MFMask({0}), params.build(), context);

  ArrayRef<float3> output_points = vector_array[0].as_typed_ref<float3>();

  Mesh *mesh = BKE_mesh_new_nomain(output_points.size(), 0, 0, 0, 0);
  for (uint i = 0; i < output_points.size(); i++) {
    copy_v3_v3(mesh->mvert[i].co, output_points[i]);
  }

  return mesh;
}
