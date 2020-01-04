#include "FN_node_tree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"
#include "BLI_string.h"

#include "mappings.h"
#include "builder.h"

namespace FN {
namespace MFGeneration {

static const FNodeInserter *try_find_node_inserter(CommonBuilderData &common, const FNode &fnode)
{
  StringRef idname = fnode.idname();
  return common.mappings.fnode_inserters.lookup_ptr(idname);
}

static const VSocketInserter *try_find_socket_inserter(CommonBuilderData &common,
                                                       const VSocket &vsocket)
{
  StringRef idname = vsocket.idname();
  return common.mappings.fsocket_inserters.lookup_ptr(idname);
}

static bool insert_nodes(CommonBuilderData &common)
{
  for (const FNode *fnode : common.function_tree.all_nodes()) {
    const FNodeInserter *inserter = try_find_node_inserter(common, *fnode);

    if (inserter != nullptr) {
      FNodeMFBuilder fnode_builder{common, *fnode};
      (*inserter)(fnode_builder);
    }
    else if (common.fsocket_data_types.has_data_sockets(*fnode)) {
      CommonBuilderBase builder{common};
      builder.add_dummy(*fnode);
    }
  }
  return true;
}

static bool insert_group_inputs(CommonBuilderData &common)
{
  for (const FGroupInput *group_input : common.function_tree.all_group_inputs()) {
    const VSocketInserter *inserter = try_find_socket_inserter(common, group_input->vsocket());

    if (inserter != nullptr) {
      VSocketMFBuilder socket_builder{common, group_input->vsocket()};
      (*inserter)(socket_builder);
      common.socket_map.add(*group_input, socket_builder.built_socket());
    }
  }
  return true;
}

static bool insert_links(CommonBuilderData &common)
{
  for (const FInputSocket *to_fsocket : common.function_tree.all_input_sockets()) {
    if (!common.fsocket_data_types.is_data_socket(*to_fsocket)) {
      continue;
    }

    ArrayRef<const FOutputSocket *> origin_sockets = to_fsocket->linked_sockets();
    ArrayRef<const FGroupInput *> origin_group_inputs = to_fsocket->linked_group_inputs();
    if (origin_sockets.size() + origin_group_inputs.size() != 1) {
      continue;
    }

    MFBuilderOutputSocket *from_socket = nullptr;

    if (origin_sockets.size() == 1) {
      if (!common.fsocket_data_types.is_data_socket(*origin_sockets[0])) {
        return false;
      }
      from_socket = &common.socket_map.lookup(*origin_sockets[0]);
    }
    else {
      if (!common.fsocket_data_types.is_data_group_input(*origin_group_inputs[0])) {
        return false;
      }
      from_socket = &common.socket_map.lookup(*origin_group_inputs[0]);
    }

    Vector<MFBuilderInputSocket *> to_sockets = common.socket_map.lookup(*to_fsocket);
    BLI_assert(to_sockets.size() >= 1);

    MFDataType from_type = from_socket->data_type();
    MFDataType to_type = to_sockets[0]->data_type();

    if (from_type != to_type) {
      const ConversionInserter *inserter = common.mappings.conversion_inserters.lookup_ptr(
          {from_type, to_type});
      if (inserter == nullptr) {
        return false;
      }
      ConversionMFBuilder builder{common};
      (*inserter)(builder);
      builder.add_link(*from_socket, builder.built_input());
      from_socket = &builder.built_output();
    }

    for (MFBuilderInputSocket *to_socket : to_sockets) {
      common.network_builder.add_link(*from_socket, *to_socket);
    }
  }

  return true;
}

static bool insert_unlinked_inputs(CommonBuilderData &common)
{
  Vector<const FInputSocket *> unlinked_data_inputs;
  for (const FInputSocket *fsocket : common.function_tree.all_input_sockets()) {
    if (common.fsocket_data_types.is_data_socket(*fsocket)) {
      if (!fsocket->is_linked()) {
        unlinked_data_inputs.append(fsocket);
      }
    }
  }

  for (const FInputSocket *fsocket : unlinked_data_inputs) {
    const VSocketInserter *inserter = common.mappings.fsocket_inserters.lookup_ptr(
        fsocket->idname());

    if (inserter == nullptr) {
      return false;
    }

    VSocketMFBuilder fsocket_builder{common, fsocket->vsocket()};
    (*inserter)(fsocket_builder);
    for (MFBuilderInputSocket *to_socket : common.socket_map.lookup(*fsocket)) {
      common.network_builder.add_link(fsocket_builder.built_socket(), *to_socket);
    }
  }

  return true;
}

static std::unique_ptr<FunctionTreeMFNetwork> build(
    const FunctionTree &function_tree,
    std::unique_ptr<MFNetworkBuilder> network_builder,
    ArrayRef<std::pair<uint, uint>> dummy_mappings)
{
  network_builder->to_dot__clipboard();

  auto network = BLI::make_unique<MFNetwork>(std::move(network_builder));

  IndexToRefMap<const MFSocket> dummy_socket_by_fsocket_id(function_tree.socket_count());
  IndexToRefMap<const FSocket> fsocket_by_dummy_socket_id(network->socket_ids().size());

  for (auto pair : dummy_mappings) {
    const FSocket &fsocket = function_tree.socket_by_id(pair.first);
    const MFSocket &socket = network->socket_by_id(pair.second);

    dummy_socket_by_fsocket_id.add_new(pair.first, socket);
    fsocket_by_dummy_socket_id.add_new(pair.second, fsocket);
  }

  DummySocketMap socket_map(function_tree,
                            *network,
                            std::move(dummy_socket_by_fsocket_id),
                            std::move(fsocket_by_dummy_socket_id));

  return BLI::make_unique<FunctionTreeMFNetwork>(
      function_tree, std::move(network), std::move(socket_map));
}

std::unique_ptr<FunctionTreeMFNetwork> generate_node_tree_multi_function_network(
    const FunctionTree &function_tree, ResourceCollector &resources)
{
  const FunctionTreeMFMappings &mappings = get_function_tree_multi_function_mappings();
  FSocketDataTypes fsocket_data_types{function_tree};
  MFSocketByFSocketMapping socket_map{function_tree};
  auto network_builder = BLI::make_unique<MFNetworkBuilder>();

  CommonBuilderData common{
      resources, mappings, fsocket_data_types, socket_map, *network_builder, function_tree};
  if (!insert_nodes(common)) {
    BLI_assert(false);
  }
  if (!insert_group_inputs(common)) {
    BLI_assert(false);
  }
  if (!insert_links(common)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(common)) {
    BLI_assert(false);
  }

  Vector<std::pair<uint, uint>> dummy_mappings = socket_map.get_dummy_mappings();

  auto function_tree_network = build(function_tree, std::move(network_builder), dummy_mappings);
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
    const FunctionTree &function_tree, ResourceCollector &resources)
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

}  // namespace MFGeneration
}  // namespace FN
