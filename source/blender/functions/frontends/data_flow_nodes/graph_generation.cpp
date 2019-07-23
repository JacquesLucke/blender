#include "graph_generation.hpp"

#include "inserters.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

namespace FN {
namespace DataFlowNodes {

static void insert_placeholder_node(BTreeGraphBuilder &builder, VirtualNode *vnode)
{
  FunctionBuilder fn_builder;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    if (builder.is_data_socket(vsocket)) {
      SharedType &type = builder.query_socket_type(vsocket);
      fn_builder.add_input(builder.query_socket_name(vsocket), type);
    }
  }
  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (builder.is_data_socket(vsocket)) {
      SharedType &type = builder.query_socket_type(vsocket);
      fn_builder.add_output(builder.query_socket_name(vsocket), type);
    }
  }

  auto fn = fn_builder.build(vnode->name());
  fn->add_body<VNodePlaceholderBody>(vnode);
  DFGB_Node *node = builder.insert_function(fn);
  builder.map_data_sockets(node, vnode);
}

static bool insert_functions_for_bnodes(BTreeGraphBuilder &builder, GraphInserters &inserters)
{
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    if (inserters.insert_node(builder, vnode)) {
      continue;
    }

    if (builder.has_data_socket(vnode)) {
      insert_placeholder_node(builder, vnode);
    }
  }
  return true;
}

static bool insert_links(BTreeGraphBuilder &builder, GraphInserters &inserters)
{
  for (VirtualSocket *input : builder.vtree().inputs_with_links()) {
    if (input->links().size() > 1) {
      continue;
    }
    BLI_assert(input->links().size() == 1);
    if (!builder.is_data_socket(input)) {
      continue;
    }
    if (!inserters.insert_link(builder, input->links()[0], input)) {
      return false;
    }
  }
  return true;
}

static void insert_unlinked_inputs(BTreeGraphBuilder &builder,
                                   UnlinkedInputsHandler &unlinked_inputs_handler)
{
  Vector<VirtualSocket *> unlinked_inputs;
  Vector<DFGB_Socket> sockets_in_builder;

  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(vsocket)) {
        DFGB_Socket socket = builder.lookup_socket(vsocket);
        if (!socket.is_linked()) {
          unlinked_inputs.append(vsocket);
          sockets_in_builder.append(socket);
        }
      }
    }
  }

  Vector<DFGB_Socket> inserted_data_origins;
  inserted_data_origins.reserve(unlinked_inputs.size());
  unlinked_inputs_handler.insert(builder, unlinked_inputs, inserted_data_origins);

  BLI_assert(unlinked_inputs.size() == inserted_data_origins.size());

  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    builder.insert_link(inserted_data_origins[i], sockets_in_builder[i]);
  }
}

static Map<VirtualSocket *, DFGraphSocket> build_mapping_for_original_sockets(
    Map<VirtualSocket *, DFGB_Socket> &socket_map,
    DataFlowGraph::ToBuilderMapping &builder_mapping)
{
  Map<VirtualSocket *, DFGraphSocket> original_socket_mapping;
  for (auto item : socket_map.items()) {
    VirtualSocket *vsocket = item.key;
    DFGraphSocket socket = builder_mapping.map_socket(item.value);
    original_socket_mapping.add_new(vsocket, socket);
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
              ArrayRef<VirtualSocket *> unlinked_inputs,
              Vector<DFGB_Socket> &r_inserted_data_origins) override
  {
    r_inserted_data_origins = std::move(m_inserters.insert_sockets(builder, unlinked_inputs));
  }
};

Optional<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree)
{
  DataFlowGraphBuilder graph_builder;
  Map<VirtualSocket *, DFGB_Socket> socket_map;

  BTreeGraphBuilder builder(vtree, graph_builder, socket_map);
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
  return VTreeDataGraph(std::move(build_result.graph),
                        build_mapping_for_original_sockets(socket_map, build_result.mapping));
}

}  // namespace DataFlowNodes
}  // namespace FN
