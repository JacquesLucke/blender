#include "graph_generation.hpp"

#include "inserters.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::IndexedNodeTree;

static void insert_placeholder_node(BTreeGraphBuilder &builder, bNode *bnode)
{
  FunctionBuilder fn_builder;
  for (bNodeSocket *bsocket : bSocketList(bnode->inputs)) {
    if (builder.is_data_socket(bsocket)) {
      SharedType &type = builder.query_socket_type(bsocket);
      fn_builder.add_input(builder.query_socket_name(bsocket), type);
    }
  }
  for (bNodeSocket *bsocket : bSocketList(bnode->outputs)) {
    if (builder.is_data_socket(bsocket)) {
      SharedType &type = builder.query_socket_type(bsocket);
      fn_builder.add_output(builder.query_socket_name(bsocket), type);
    }
  }

  auto fn = fn_builder.build(bnode->name);
  DFGB_Node *node = builder.insert_function(fn);
  builder.map_data_sockets(node, bnode);
}

static bool insert_functions_for_bnodes(BTreeGraphBuilder &builder, GraphInserters &inserters)
{
  for (bNode *bnode : builder.indexed_btree().actual_nodes()) {
    if (inserters.insert_node(builder, bnode)) {
      continue;
    }

    if (builder.has_data_socket(bnode)) {
      insert_placeholder_node(builder, bnode);
    }
  }
  return true;
}

static bool insert_links(BTreeGraphBuilder &builder, GraphInserters &inserters)
{
  IndexedNodeTree indexed_btree(builder.btree());
  for (auto &link : indexed_btree.single_origin_links()) {
    if (!builder.is_data_socket(link.to)) {
      continue;
    }
    if (!inserters.insert_link(builder, link.from, link.to, link.source_link)) {
      return false;
    }
  }
  return true;
}

static void insert_unlinked_inputs(BTreeGraphBuilder &builder, GraphInserters &inserters)
{
  SmallVector<bNodeSocket *> unlinked_inputs;
  DFGB_SocketVector node_inputs;

  for (bNode *bnode : builder.indexed_btree().actual_nodes()) {
    for (bNodeSocket *bsocket : bSocketList(bnode->inputs)) {
      if (builder.is_data_socket(bsocket)) {
        DFGB_Socket socket = builder.lookup_socket(bsocket);
        if (!socket.is_linked()) {
          unlinked_inputs.append(bsocket);
          node_inputs.append(socket);
        }
      }
    }
  }

  DFGB_SocketVector new_origins = inserters.insert_sockets(builder, unlinked_inputs);
  BLI_assert(unlinked_inputs.size() == new_origins.size());

  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    builder.insert_link(new_origins[i], node_inputs[i]);
  }
}

static FunctionGraph finalize_function_graph(DataFlowGraphBuilder &builder,
                                             DFGB_SocketVector input_sockets,
                                             DFGB_SocketVector output_sockets)
{
  auto build_result = DataFlowGraph::FromBuilder(builder);
  auto &builder_mapping = build_result.mapping;

  DFGraphSocketSetVector inputs, outputs;

  for (DFGB_Socket socket : input_sockets) {
    inputs.add(builder_mapping.map_socket(socket));
  }
  for (DFGB_Socket socket : output_sockets) {
    outputs.add(builder_mapping.map_socket(socket));
  }

  return FunctionGraph(build_result.graph, inputs, outputs);
}

static void find_interface_sockets(BTreeGraphBuilder &builder,
                                   DFGB_SocketVector &r_inputs,
                                   DFGB_SocketVector &r_outputs)
{
  bNode *input_node =
      builder.indexed_btree().nodes_with_idname("fn_FunctionInputNode").get(0, nullptr);
  bNode *output_node =
      builder.indexed_btree().nodes_with_idname("fn_FunctionOutputNode").get(0, nullptr);

  if (input_node != nullptr) {
    for (bNodeSocket *bsocket : bSocketList(input_node->outputs)) {
      if (builder.is_data_socket(bsocket)) {
        r_inputs.append(builder.lookup_socket(bsocket));
      }
    }
  }

  if (output_node != nullptr) {
    for (bNodeSocket *bsocket : bSocketList(output_node->inputs)) {
      if (builder.is_data_socket(bsocket)) {
        r_outputs.append(builder.lookup_socket(bsocket));
      }
    }
  }
}

Optional<FunctionGraph> generate_function_graph(bNodeTree *btree)
{
  IndexedNodeTree indexed_btree(btree);

  DataFlowGraphBuilder graph_builder;
  SmallMap<struct bNodeSocket *, DFGB_Socket> socket_map;

  BTreeGraphBuilder builder(indexed_btree, graph_builder, socket_map);
  GraphInserters &inserters = get_standard_inserters();

  if (!insert_functions_for_bnodes(builder, inserters)) {
    return {};
  }

  if (!insert_links(builder, inserters)) {
    return {};
  }

  insert_unlinked_inputs(builder, inserters);

  DFGB_SocketVector input_sockets;
  DFGB_SocketVector output_sockets;
  find_interface_sockets(builder, input_sockets, output_sockets);

  return finalize_function_graph(graph_builder, input_sockets, output_sockets);
}

}  // namespace DataFlowNodes
}  // namespace FN
