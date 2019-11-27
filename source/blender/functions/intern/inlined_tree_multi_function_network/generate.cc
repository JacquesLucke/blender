#include "FN_inlined_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"
#include "BLI_string.h"

#include "mappings.h"
#include "builder.h"

namespace FN {

using BKE::XGroupInput;

static bool insert_nodes(VTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  const InlinedNodeTree &inlined_tree = builder.inlined_tree();

  for (const XNode *xnode : inlined_tree.all_nodes()) {
    StringRef idname = xnode->idname();
    const InsertVNodeFunction *inserter = mappings.xnode_inserters.lookup_ptr(idname);

    if (inserter != nullptr) {
      VNodeMFNetworkBuilder xnode_builder{builder, *xnode};

      (*inserter)(xnode_builder);
      builder.assert_xnode_is_mapped_correctly(*xnode);
    }
    else if (builder.has_data_sockets(*xnode)) {
      builder.add_dummy(*xnode);
    }
  }

  for (const XGroupInput *group_input : inlined_tree.all_group_inputs()) {
    VSocketMFNetworkBuilder socket_builder{builder, group_input->vsocket()};
    const InsertVSocketFunction *inserter = mappings.xsocket_inserters.lookup_ptr(
        group_input->vsocket().idname());
    if (inserter != nullptr) {
      (*inserter)(socket_builder);
      builder.map_group_input(*group_input, socket_builder.built_socket());
    }
  }

  return true;
}

static bool insert_links(VTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  for (const XInputSocket *to_xsocket : builder.inlined_tree().all_input_sockets()) {
    if (!builder.is_data_socket(*to_xsocket)) {
      continue;
    }

    ArrayRef<const XOutputSocket *> origin_sockets = to_xsocket->linked_sockets();
    ArrayRef<const XGroupInput *> origin_group_inputs = to_xsocket->linked_group_inputs();
    if (origin_sockets.size() + origin_group_inputs.size() != 1) {
      continue;
    }

    MFBuilderOutputSocket *from_socket = nullptr;
    StringRef from_idname;

    if (origin_sockets.size() == 1) {
      if (!builder.is_data_socket(*origin_sockets[0])) {
        return false;
      }
      from_socket = &builder.lookup_socket(*origin_sockets[0]);
      from_idname = origin_sockets[0]->idname();
    }
    else {
      if (!builder.is_data_group_input(*origin_group_inputs[0])) {
        return false;
      }
      from_socket = &builder.lookup_group_input(*origin_group_inputs[0]);
      from_idname = origin_group_inputs[0]->vsocket().idname();
    }

    Vector<MFBuilderInputSocket *> to_sockets = builder.lookup_socket(*to_xsocket);
    BLI_assert(to_sockets.size() >= 1);

    MFDataType from_type = from_socket->data_type();
    MFDataType to_type = to_sockets[0]->data_type();

    if (from_type != to_type) {
      const InsertImplicitConversionFunction *inserter = mappings.conversion_inserters.lookup_ptr(
          {from_idname, to_xsocket->idname()});
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
  Vector<const XInputSocket *> unlinked_data_inputs;
  for (const XInputSocket *xsocket : builder.inlined_tree().all_input_sockets()) {
    if (builder.is_data_socket(*xsocket)) {
      if (!xsocket->is_linked()) {
        unlinked_data_inputs.append(xsocket);
      }
    }
  }

  for (const XInputSocket *xsocket : unlinked_data_inputs) {
    const InsertVSocketFunction *inserter = mappings.xsocket_inserters.lookup_ptr(
        xsocket->idname());

    if (inserter == nullptr) {
      return false;
    }

    VSocketMFNetworkBuilder xsocket_builder{builder, xsocket->vsocket()};
    (*inserter)(xsocket_builder);
    for (MFBuilderInputSocket *to_socket : builder.lookup_socket(*xsocket)) {
      builder.add_link(xsocket_builder.built_socket(), *to_socket);
    }
  }

  return true;
}

std::unique_ptr<VTreeMFNetwork> generate_inlined_tree_multi_function_network(
    const InlinedNodeTree &inlined_tree, ResourceCollector &resources)
{
  const VTreeMultiFunctionMappings &mappings = get_inlined_tree_multi_function_mappings();
  PreprocessedVTreeMFData preprocessed_data{inlined_tree};

  VTreeMFNetworkBuilder builder(inlined_tree, preprocessed_data, mappings, resources);
  if (!insert_nodes(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_links(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(builder, mappings)) {
    BLI_assert(false);
  }

  auto inlined_tree_network = builder.build();
  return inlined_tree_network;
}

static bool cmp_group_interface_nodes(const XNode *a, const XNode *b)
{
  int a_index = RNA_int_get(a->rna(), "sort_index");
  int b_index = RNA_int_get(b->rna(), "sort_index");
  if (a_index < b_index) {
    return true;
  }

  /* TODO: Match sorting with Python. */
  return BLI_strcasecmp(a->name().data(), b->name().data()) == -1;
}

std::unique_ptr<MF_EvaluateNetwork> generate_inlined_tree_multi_function(
    const InlinedNodeTree &inlined_tree, ResourceCollector &resources)
{
  std::unique_ptr<VTreeMFNetwork> network = generate_inlined_tree_multi_function_network(
      inlined_tree, resources);

  Vector<const XNode *> input_xnodes = inlined_tree.nodes_with_idname("fn_GroupDataInputNode");
  Vector<const XNode *> output_xnodes = inlined_tree.nodes_with_idname("fn_GroupDataOutputNode");

  std::sort(input_xnodes.begin(), input_xnodes.end(), cmp_group_interface_nodes);
  std::sort(output_xnodes.begin(), output_xnodes.end(), cmp_group_interface_nodes);

  Vector<const MFOutputSocket *> function_inputs;
  Vector<const MFInputSocket *> function_outputs;

  for (const XNode *xnode : input_xnodes) {
    function_inputs.append(&network->lookup_dummy_socket(xnode->output(0)));
  }

  for (const XNode *xnode : output_xnodes) {
    function_outputs.append(&network->lookup_dummy_socket(xnode->input(0)));
  }

  auto function = BLI::make_unique<MF_EvaluateNetwork>(std::move(function_inputs),
                                                       std::move(function_outputs));
  resources.add(std::move(network), "VTree Multi Function Network");
  return function;
}

}  // namespace FN
