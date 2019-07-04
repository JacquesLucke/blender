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

static void insert_unlinked_inputs(BTreeGraphBuilder &builder,
                                   UnlinkedInputsHandler &unlinked_inputs_handler)
{
  SmallVector<bNodeSocket *> unlinked_inputs;
  SmallVector<DFGB_Socket> sockets_in_builder;

  for (bNode *bnode : builder.indexed_btree().actual_nodes()) {
    for (bNodeSocket *bsocket : bSocketList(bnode->inputs)) {
      if (builder.is_data_socket(bsocket)) {
        DFGB_Socket socket = builder.lookup_socket(bsocket);
        if (!socket.is_linked()) {
          unlinked_inputs.append(bsocket);
          sockets_in_builder.append(socket);
        }
      }
    }
  }

  DFGB_SocketVector inserted_data_origins;
  inserted_data_origins.reserve(unlinked_inputs.size());
  unlinked_inputs_handler.insert(builder, unlinked_inputs, inserted_data_origins);

  BLI_assert(unlinked_inputs.size() == inserted_data_origins.size());

  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    builder.insert_link(inserted_data_origins[i], sockets_in_builder[i]);
  }
}

static void find_interface_sockets(IndexedNodeTree &indexed_btree,
                                   GeneratedGraph &graph,
                                   DFGraphSocketSetVector &r_inputs,
                                   DFGraphSocketSetVector &r_outputs)
{
  bNode *input_node = indexed_btree.nodes_with_idname("fn_FunctionInputNode").get(0, nullptr);
  bNode *output_node = indexed_btree.nodes_with_idname("fn_FunctionOutputNode").get(0, nullptr);

  if (input_node != nullptr) {
    for (bNodeSocket *bsocket : bSocketList(input_node->outputs)) {
      if (bsocket != (bNodeSocket *)input_node->outputs.last) {
        r_inputs.add_new(graph.lookup_socket(bsocket));
      }
    }
  }

  if (output_node != nullptr) {
    for (bNodeSocket *bsocket : bSocketList(output_node->inputs)) {
      if (bsocket != (bNodeSocket *)output_node->inputs.last) {
        r_outputs.add_new(graph.lookup_socket(bsocket));
      }
    }
  }
}

static SmallMap<bNodeSocket *, DFGraphSocket> build_mapping_for_original_sockets(
    SmallMap<bNodeSocket *, DFGB_Socket> &socket_map,
    DataFlowGraph::ToBuilderMapping &builder_mapping)
{
  SmallMap<bNodeSocket *, DFGraphSocket> original_socket_mapping;
  for (auto item : socket_map.items()) {
    bNodeSocket *bsocket = item.key;
    DFGraphSocket socket = builder_mapping.map_socket(item.value);
    original_socket_mapping.add_new(bsocket, socket);
  }
  return original_socket_mapping;
}

class BasicUnlinkedInputsHandler : public UnlinkedInputsHandler {
 private:
  GraphInserters &m_inserters;

 public:
  BasicUnlinkedInputsHandler(GraphInserters &inserters) : m_inserters(inserters)
  {
  }

  void insert(BTreeGraphBuilder &builder,
              ArrayRef<bNodeSocket *> unlinked_inputs,
              DFGB_SocketVector &r_inserted_data_origins) override
  {
    r_inserted_data_origins = std::move(m_inserters.insert_sockets(builder, unlinked_inputs));
  }
};

Optional<GeneratedGraph> generate_graph(IndexedNodeTree &indexed_btree)
{
  DataFlowGraphBuilder graph_builder;
  SmallMap<bNodeSocket *, DFGB_Socket> socket_map;

  BTreeGraphBuilder builder(indexed_btree, graph_builder, socket_map);
  GraphInserters &inserters = get_standard_inserters();

  if (!insert_functions_for_bnodes(builder, inserters)) {
    return {};
  }

  if (!insert_links(builder, inserters)) {
    return {};
  }

  BasicUnlinkedInputsHandler unlinked_inputs_handler(inserters);

  insert_unlinked_inputs(builder, unlinked_inputs_handler);

  auto build_result = DataFlowGraph::FromBuilder(graph_builder);
  return GeneratedGraph(std::move(build_result.graph),
                        build_mapping_for_original_sockets(socket_map, build_result.mapping));
}

Optional<FunctionGraph> generate_function_graph(IndexedNodeTree &indexed_btree)
{
  Optional<GeneratedGraph> generated_graph_ = generate_graph(indexed_btree);
  if (!generated_graph_.has_value()) {
    return {};
  }

  GeneratedGraph &generated_graph = generated_graph_.value();

  DFGraphSocketSetVector input_sockets;
  DFGraphSocketSetVector output_sockets;
  find_interface_sockets(indexed_btree, generated_graph, input_sockets, output_sockets);

  return FunctionGraph(generated_graph.graph(), input_sockets, output_sockets);
}

}  // namespace DataFlowNodes
}  // namespace FN
