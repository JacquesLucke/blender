#include "FN_node_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"
#include "BLI_string.h"

#include "mappings.h"
#include "builder.h"

namespace FN {

static bool insert_nodes(FunctionTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  const FunctionNodeTree &function_tree = builder.function_tree();

  for (const FNode *fnode : function_tree.all_nodes()) {
    StringRef idname = fnode->idname();
    const InsertVNodeFunction *inserter = mappings.fnode_inserters.lookup_ptr(idname);

    if (inserter != nullptr) {
      FNodeMFNetworkBuilder fnode_builder{builder, *fnode};

      (*inserter)(fnode_builder);
      builder.assert_fnode_is_mapped_correctly(*fnode);
    }
    else if (builder.has_data_sockets(*fnode)) {
      builder.add_dummy(*fnode);
    }
  }

  for (const FGroupInput *group_input : function_tree.all_group_inputs()) {
    VSocketMFNetworkBuilder socket_builder{builder, group_input->vsocket()};
    const InsertVSocketFunction *inserter = mappings.fsocket_inserters.lookup_ptr(
        group_input->vsocket().idname());
    if (inserter != nullptr) {
      (*inserter)(socket_builder);
      builder.map_group_input(*group_input, socket_builder.built_socket());
    }
  }

  return true;
}

static bool insert_links(FunctionTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  for (const FInputSocket *to_fsocket : builder.function_tree().all_input_sockets()) {
    if (!builder.is_data_socket(*to_fsocket)) {
      continue;
    }

    ArrayRef<const FOutputSocket *> origin_sockets = to_fsocket->linked_sockets();
    ArrayRef<const FGroupInput *> origin_group_inputs = to_fsocket->linked_group_inputs();
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

    Vector<MFBuilderInputSocket *> to_sockets = builder.lookup_socket(*to_fsocket);
    BLI_assert(to_sockets.size() >= 1);

    MFDataType from_type = from_socket->data_type();
    MFDataType to_type = to_sockets[0]->data_type();

    if (from_type != to_type) {
      const InsertImplicitConversionFunction *inserter = mappings.conversion_inserters.lookup_ptr(
          {from_idname, to_fsocket->idname()});
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

static bool insert_unlinked_inputs(FunctionTreeMFNetworkBuilder &builder,
                                   const VTreeMultiFunctionMappings &mappings)
{
  Vector<const FInputSocket *> unlinked_data_inputs;
  for (const FInputSocket *fsocket : builder.function_tree().all_input_sockets()) {
    if (builder.is_data_socket(*fsocket)) {
      if (!fsocket->is_linked()) {
        unlinked_data_inputs.append(fsocket);
      }
    }
  }

  for (const FInputSocket *fsocket : unlinked_data_inputs) {
    const InsertVSocketFunction *inserter = mappings.fsocket_inserters.lookup_ptr(
        fsocket->idname());

    if (inserter == nullptr) {
      return false;
    }

    VSocketMFNetworkBuilder fsocket_builder{builder, fsocket->vsocket()};
    (*inserter)(fsocket_builder);
    for (MFBuilderInputSocket *to_socket : builder.lookup_socket(*fsocket)) {
      builder.add_link(fsocket_builder.built_socket(), *to_socket);
    }
  }

  return true;
}

static std::unique_ptr<FunctionTreeMFNetwork> build(
    const FunctionNodeTree &function_tree,
    std::unique_ptr<MFNetworkBuilder> network_builder,
    const IndexToRefMultiMap<MFBuilderSocket> &sockets_by_fsocket_id)
{
  // m_builder.to_dot__clipboard();

  Vector<std::pair<uint, uint>> m_dummy_mappings;
  for (uint fsocket_id : IndexRange(function_tree.socket_count())) {
    ArrayRef<MFBuilderSocket *> mapped_sockets = sockets_by_fsocket_id.lookup(fsocket_id);
    if (mapped_sockets.size() == 1) {
      MFBuilderSocket &socket = *mapped_sockets[0];
      if (socket.node().is_dummy()) {
        m_dummy_mappings.append({fsocket_id, socket.id()});
      }
    }
  }

  auto network = BLI::make_unique<MFNetwork>(std::move(network_builder));

  IndexToRefMap<const MFSocket> dummy_socket_by_fsocket_id(function_tree.socket_count());
  IndexToRefMap<const FSocket> fsocket_by_dummy_socket_id(network->socket_ids().size());

  for (auto pair : m_dummy_mappings) {
    const FSocket &fsocket = function_tree.socket_by_id(pair.first);
    const MFSocket &socket = network->socket_by_id(pair.second);

    dummy_socket_by_fsocket_id.add_new(pair.first, socket);
    fsocket_by_dummy_socket_id.add_new(pair.second, fsocket);
  }

  InlinedTreeMFSocketMap socket_map(function_tree,
                                    *network,
                                    std::move(dummy_socket_by_fsocket_id),
                                    std::move(fsocket_by_dummy_socket_id));

  return BLI::make_unique<FunctionTreeMFNetwork>(
      function_tree, std::move(network), std::move(socket_map));
}

std::unique_ptr<FunctionTreeMFNetwork> generate_node_tree_multi_function_network(
    const FunctionNodeTree &function_tree, ResourceCollector &resources)
{
  const VTreeMultiFunctionMappings &mappings = get_function_tree_multi_function_mappings();
  FSocketDataTypes fsocket_data_types{function_tree};
  IndexToRefMultiMap<MFBuilderSocket> sockets_by_fsocket_id(function_tree.all_sockets().size());
  IndexToRefMap<MFBuilderOutputSocket> socket_by_group_input_id(
      function_tree.all_group_inputs().size());
  auto network_builder = BLI::make_unique<MFNetworkBuilder>();

  FunctionTreeMFNetworkBuilder builder(function_tree,
                                       fsocket_data_types,
                                       mappings,
                                       resources,
                                       sockets_by_fsocket_id,
                                       socket_by_group_input_id,
                                       *network_builder);
  if (!insert_nodes(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_links(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(builder, mappings)) {
    BLI_assert(false);
  }

  auto function_tree_network = build(
      function_tree, std::move(network_builder), sockets_by_fsocket_id);
  return function_tree_network;
}

static bool cmp_group_interface_nodes(const FNode *a, const FNode *b)
{
  int a_index = RNA_int_get(a->rna(), "sort_index");
  int b_index = RNA_int_get(b->rna(), "sort_index");
  if (a_index < b_index) {
    return true;
  }

  /* TODO: Match sorting with Python. */
  return BLI_strcasecmp(a->name().data(), b->name().data()) == -1;
}

std::unique_ptr<MF_EvaluateNetwork> generate_node_tree_multi_function(
    const FunctionNodeTree &function_tree, ResourceCollector &resources)
{
  std::unique_ptr<FunctionTreeMFNetwork> network = generate_node_tree_multi_function_network(
      function_tree, resources);

  Vector<const FNode *> input_fnodes = function_tree.nodes_with_idname("fn_GroupInputNode");
  Vector<const FNode *> output_fnodes = function_tree.nodes_with_idname("fn_GroupOutputNode");

  std::sort(input_fnodes.begin(), input_fnodes.end(), cmp_group_interface_nodes);
  std::sort(output_fnodes.begin(), output_fnodes.end(), cmp_group_interface_nodes);

  Vector<const MFOutputSocket *> function_inputs;
  Vector<const MFInputSocket *> function_outputs;

  for (const FNode *fnode : input_fnodes) {
    function_inputs.append(&network->lookup_dummy_socket(fnode->output(0)));
  }

  for (const FNode *fnode : output_fnodes) {
    function_outputs.append(&network->lookup_dummy_socket(fnode->input(0)));
  }

  auto function = BLI::make_unique<MF_EvaluateNetwork>(std::move(function_inputs),
                                                       std::move(function_outputs));
  resources.add(std::move(network), "VTree Multi Function Network");
  return function;
}

}  // namespace FN
