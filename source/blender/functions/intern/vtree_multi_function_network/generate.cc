#include "FN_vtree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"
#include "BLI_string.h"

#include "mappings.h"
#include "builder.h"

namespace FN {

static bool insert_nodes(VTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  const VirtualNodeTree &vtree = builder.vtree();

  for (const VNode *vnode : vtree.nodes()) {
    StringRef idname = vnode->idname();
    const InsertVNodeFunction *inserter = mappings.vnode_inserters.lookup_ptr(idname);

    if (inserter != nullptr) {
      (*inserter)(builder, *vnode);
#ifdef DEBUG
      builder.assert_vnode_is_mapped_correctly(*vnode);
#endif
    }
    else if (builder.has_data_sockets(*vnode)) {
      builder.add_dummy(*vnode);
    }
  }

  return true;
}

static bool insert_links(VTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  for (const VInputSocket *to_vsocket : builder.vtree().all_input_sockets()) {
    ArrayRef<const VOutputSocket *> origins = to_vsocket->linked_sockets();
    if (origins.size() != 1) {
      continue;
    }

    if (!builder.is_data_socket(*to_vsocket)) {
      continue;
    }

    const VOutputSocket *from_vsocket = origins[0];
    if (!builder.is_data_socket(*from_vsocket)) {
      return false;
    }

    MFBuilderOutputSocket *from_socket = &builder.lookup_socket(*from_vsocket);
    Vector<MFBuilderInputSocket *> to_sockets = builder.lookup_socket(*to_vsocket);
    BLI_assert(to_sockets.size() >= 1);

    MFDataType from_type = from_socket->data_type();
    MFDataType to_type = to_sockets[0]->data_type();

    if (from_type != to_type) {
      const InsertImplicitConversionFunction *inserter = mappings.conversion_inserters.lookup_ptr(
          {from_vsocket->idname(), to_vsocket->idname()});
      if (inserter == nullptr) {
        return false;
      }
      auto new_sockets = (*inserter)(builder);
      builder.add_link(*from_socket, *new_sockets.first);
      from_socket = new_sockets.second;
    }

    for (MFBuilderInputSocket *to_socket : to_sockets) {
      builder.add_link(*from_socket, *to_socket);
    }
  }

  return true;
}

static bool insert_unlinked_inputs(VTreeMFNetworkBuilder &builder,
                                   const VTreeMultiFunctionMappings &mappings)
{
  Vector<const VInputSocket *> unlinked_data_inputs;
  for (const VInputSocket *vsocket : builder.vtree().all_input_sockets()) {
    if (builder.is_data_socket(*vsocket)) {
      if (!vsocket->is_linked()) {
        unlinked_data_inputs.append(vsocket);
      }
    }
  }

  for (const VInputSocket *vsocket : unlinked_data_inputs) {
    const InsertVSocketFunction *inserter = mappings.vsocket_inserters.lookup_ptr(
        vsocket->idname());

    if (inserter == nullptr) {
      return false;
    }

    VSocketMFNetworkBuilder vsocket_builder{builder, *vsocket};
    (*inserter)(vsocket_builder);
    for (MFBuilderInputSocket *to_socket : builder.lookup_socket(*vsocket)) {
      builder.add_link(vsocket_builder.built_socket(), *to_socket);
    }
  }

  return true;
}

std::unique_ptr<VTreeMFNetwork> generate_vtree_multi_function_network(const VirtualNodeTree &vtree,
                                                                      ResourceCollector &resources)
{
  const VTreeMultiFunctionMappings &mappings = get_vtree_multi_function_mappings();
  PreprocessedVTreeMFData preprocessed_data{vtree};

  VTreeMFNetworkBuilder builder(vtree, preprocessed_data, mappings, resources);
  if (!insert_nodes(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_links(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(builder, mappings)) {
    BLI_assert(false);
  }

  auto vtree_network = builder.build();
  return vtree_network;
}

static bool cmp_group_interface_nodes(const VNode *a, const VNode *b)
{
  int a_index = RNA_int_get(a->rna(), "sort_index");
  int b_index = RNA_int_get(b->rna(), "sort_index");
  if (a_index < b_index) {
    return true;
  }

  /* TODO: Match sorting with Python. */
  return BLI_strcasecmp(a->name().data(), b->name().data()) == -1;
}

std::unique_ptr<MF_EvaluateNetwork> generate_vtree_multi_function(const VirtualNodeTree &vtree,
                                                                  ResourceCollector &resources)
{
  std::unique_ptr<VTreeMFNetwork> network = generate_vtree_multi_function_network(vtree,
                                                                                  resources);

  Vector<const VNode *> input_vnodes = vtree.nodes_with_idname("fn_GroupDataInputNode");
  Vector<const VNode *> output_vnodes = vtree.nodes_with_idname("fn_GroupDataOutputNode");

  std::sort(input_vnodes.begin(), input_vnodes.end(), cmp_group_interface_nodes);
  std::sort(output_vnodes.begin(), output_vnodes.end(), cmp_group_interface_nodes);

  Vector<const MFOutputSocket *> function_inputs;
  Vector<const MFInputSocket *> function_outputs;

  for (const VNode *vnode : input_vnodes) {
    function_inputs.append(&network->lookup_dummy_socket(vnode->output(0)));
  }

  for (const VNode *vnode : output_vnodes) {
    function_outputs.append(&network->lookup_dummy_socket(vnode->input(0)));
  }

  auto function = BLI::make_unique<MF_EvaluateNetwork>(std::move(function_inputs),
                                                       std::move(function_outputs));
  resources.add(std::move(network), "VTree Multi Function Network");
  return function;
}

}  // namespace FN
