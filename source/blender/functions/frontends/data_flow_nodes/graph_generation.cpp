#include "graph_generation.hpp"

#include "inserters.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

namespace FN {
namespace DataFlowNodes {

using BKE::IndexedNodeTree;

static bool is_input_node(const bNode *bnode)
{
  return STREQ(bnode->idname, "fn_FunctionInputNode");
}

static bool is_output_node(const bNode *bnode)
{
  return STREQ(bnode->idname, "fn_FunctionOutputNode");
}

static bool is_reroute_node(const bNode *bnode)
{
  return STREQ(bnode->idname, "NodeReroute");
}

static bNode *find_input_node(bNodeTree *btree)
{
  for (bNode *bnode : bNodeList(&btree->nodes)) {
    if (is_input_node(bnode)) {
      return bnode;
    }
  }
  return nullptr;
}

static bNode *find_output_node(bNodeTree *btree)
{
  for (bNode *bnode : bNodeList(&btree->nodes)) {
    if (is_output_node(bnode)) {
      return bnode;
    }
  }
  return nullptr;
}

static void insert_input_node(BTreeGraphBuilder &builder, bNode *bnode)
{
  FunctionBuilder fn_builder;
  for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
    if (builder.is_data_socket(bsocket)) {
      SharedType &type = builder.query_socket_type(bsocket);
      fn_builder.add_output(builder.query_socket_name(bsocket), type);
    }
  }

  auto fn = fn_builder.build("Function Input");
  DFGB_Node *node = builder.insert_function(fn);
  builder.map_data_sockets(node, bnode);
}

static void insert_output_node(BTreeGraphBuilder &builder, bNode *bnode)
{
  FunctionBuilder fn_builder;
  for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
    if (builder.is_data_socket(bsocket)) {
      SharedType &type = builder.query_socket_type(bsocket);
      fn_builder.add_input(builder.query_socket_name(bsocket), type);
    }
  }

  auto fn = fn_builder.build("Function Output");
  DFGB_Node *node = builder.insert_function(fn);
  builder.map_data_sockets(node, bnode);
}

struct BSocketLink {
  bNodeSocket *from;
  bNodeSocket *to;
  bNodeLink *optional_source_link;

  BSocketLink(bNodeSocket *from, bNodeSocket *to, bNodeLink *link = nullptr)
      : from(from), to(to), optional_source_link(link)
  {
  }
};

static bool insert_functions_for_bnodes(BTreeGraphBuilder &builder, GraphInserters &inserters)
{
  for (bNode *bnode : bNodeList(&builder.btree()->nodes)) {
    if (is_input_node(bnode) || is_output_node(bnode)) {
      continue;
    }
    if (is_reroute_node(bnode)) {
      continue;
    }

    if (!inserters.insert_node(builder, bnode)) {
      return false;
    }
  }
  return true;
}

static DFGB_SocketVector insert_function_input(BTreeGraphBuilder &builder)
{
  bNode *input_bnode = find_input_node(builder.btree());
  if (input_bnode == nullptr) {
    return {};
  }

  DFGB_SocketVector input_sockets;
  insert_input_node(builder, input_bnode);
  for (bNodeSocket *bsocket : bSocketList(&input_bnode->outputs)) {
    if (builder.is_data_socket(bsocket)) {
      input_sockets.append(builder.lookup_socket(bsocket));
    }
  }
  return input_sockets;
}

static DFGB_SocketVector insert_function_output(BTreeGraphBuilder &builder)
{
  bNode *output_bnode = find_output_node(builder.btree());
  if (output_bnode == nullptr) {
    return {};
  }

  DFGB_SocketVector output_sockets;
  insert_output_node(builder, output_bnode);
  for (bNodeSocket *bsocket : bSocketList(&output_bnode->inputs)) {
    if (builder.is_data_socket(bsocket)) {
      output_sockets.append(builder.lookup_socket(bsocket));
    }
  }
  return output_sockets;
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

  for (bNode *bnode : bNodeList(&builder.btree()->nodes)) {
    for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
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
  DataFlowGraph::ToBuilderMapping builder_mapping;
  auto compact_graph = DataFlowGraph::FromBuilder(builder, builder_mapping);

  DFGraphSocketSetVector inputs, outputs;

  for (DFGB_Socket socket : input_sockets) {
    inputs.add(builder_mapping.map_socket(socket));
  }
  for (DFGB_Socket socket : output_sockets) {
    outputs.add(builder_mapping.map_socket(socket));
  }

  return FunctionGraph(compact_graph, inputs, outputs);
}

Optional<FunctionGraph> generate_function_graph(bNodeTree *btree)
{
  DataFlowGraphBuilder graph_builder;
  SmallMap<struct bNodeSocket *, DFGB_Socket> socket_map;

  BTreeGraphBuilder builder(btree, graph_builder, socket_map);
  GraphInserters &inserters = get_standard_inserters();

  if (!insert_functions_for_bnodes(builder, inserters)) {
    return {};
  }

  DFGB_SocketVector input_sockets = insert_function_input(builder);
  DFGB_SocketVector output_sockets = insert_function_output(builder);

  if (!insert_links(builder, inserters)) {
    return {};
  }

  insert_unlinked_inputs(builder, inserters);

  return finalize_function_graph(graph_builder, input_sockets, output_sockets);
}

}  // namespace DataFlowNodes
}  // namespace FN
