#include "graph_generation.hpp"

#include "inserters.hpp"
#include "util_wrappers.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

namespace FN {
namespace DataFlowNodes {

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

static void find_interface_nodes(bNodeTree *btree, bNode **r_input, bNode **r_output)
{
  bNode *input = nullptr;
  bNode *output = nullptr;

  for (bNode *bnode : bNodeList(&btree->nodes)) {
    if (is_input_node(bnode))
      input = bnode;
    if (is_output_node(bnode))
      output = bnode;
  }

  *r_input = input;
  *r_output = output;
}

static void insert_input_node(GraphBuilder &builder, bNode *bnode)
{
  OutputParameters outputs;
  for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
    if (builder.is_data_socket(bsocket)) {
      SharedType &type = builder.query_socket_type(bsocket);
      outputs.append(OutputParameter(builder.query_socket_name(bsocket), type));
    }
  }

  auto fn = SharedFunction::New("Function Input", Signature({}, outputs));
  Node *node = builder.insert_function(fn);
  builder.map_data_sockets(node, bnode);
}

static void insert_output_node(GraphBuilder &builder, bNode *bnode)
{
  InputParameters inputs;
  for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
    if (builder.is_data_socket(bsocket)) {
      SharedType &type = builder.query_socket_type(bsocket);
      inputs.append(InputParameter(builder.query_socket_name(bsocket), type));
    }
  }

  auto fn = SharedFunction::New("Function Output", Signature(inputs, {}));
  Node *node = builder.insert_function(fn);
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

using BSocketMapping = SmallMap<bNodeSocket *, bNodeSocket *>;
using BSocketLinkVector = SmallVector<BSocketLink>;

class TreeData {
 private:
  SmallMap<bNodeSocket *, bNode *> m_node_by_socket;
  BSocketMapping m_direct_origin_socket;
  BSocketLinkVector m_data_links;

 public:
  TreeData(bNodeTree *btree)
  {
    for (bNode *bnode : bNodeList(&btree->nodes)) {
      for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
        m_node_by_socket.add(bsocket, bnode);
      }
      for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
        m_node_by_socket.add(bsocket, bnode);
      }
    }

    for (bNodeLink *blink : bLinkList(&btree->links)) {
      BLI_assert(!m_direct_origin_socket.contains(blink->tosock));
      m_direct_origin_socket.add(blink->tosock, blink->fromsock);
    }

    for (bNodeLink *blink : bLinkList(&btree->links)) {
      bNodeSocket *target = blink->tosock;
      bNode *target_node = m_node_by_socket.lookup(target);
      if (is_reroute_node(target_node)) {
        continue;
      }
      bNodeSocket *origin = this->try_find_data_origin(target);
      if (origin != nullptr) {
        m_data_links.append(BSocketLink(origin, target, blink));
      }
    }
  }

  const BSocketLinkVector &data_origins()
  {
    return m_data_links;
  }

 private:
  bNodeSocket *try_find_data_origin(bNodeSocket *bsocket)
  {
    BLI_assert(bsocket->in_out == SOCK_IN);
    if (m_direct_origin_socket.contains(bsocket)) {
      bNodeSocket *origin = m_direct_origin_socket.lookup(bsocket);
      bNode *origin_node = m_node_by_socket.lookup(origin);
      if (is_reroute_node(origin_node)) {
        return this->try_find_data_origin((bNodeSocket *)origin_node->inputs.first);
      }
      else {
        return origin;
      }
    }
    else {
      return nullptr;
    }
  }
};

Optional<CompactFunctionGraph> generate_function_graph(struct bNodeTree *btree)
{
  auto graph = SharedDataFlowGraph::New();
  SocketMap socket_map;

  GraphBuilder builder(btree, graph, socket_map);
  GraphInserters &inserters = get_standard_inserters();

  bNode *input_node;
  bNode *output_node;
  find_interface_nodes(btree, &input_node, &output_node);

  for (bNode *bnode : bNodeList(&btree->nodes)) {
    if (bnode == input_node || bnode == output_node) {
      continue;
    }
    if (is_reroute_node(bnode)) {
      continue;
    }

    if (!inserters.insert_node(builder, bnode)) {
      return {};
    }
  }

  SocketVector input_sockets;
  SocketVector output_sockets;

  if (input_node != nullptr) {
    insert_input_node(builder, input_node);
    for (bNodeSocket *bsocket : bSocketList(&input_node->outputs)) {
      if (builder.is_data_socket(bsocket)) {
        input_sockets.append(socket_map.lookup(bsocket));
      }
    }
  }
  if (output_node != nullptr) {
    insert_output_node(builder, output_node);
    for (bNodeSocket *bsocket : bSocketList(&output_node->inputs)) {
      if (builder.is_data_socket(bsocket)) {
        output_sockets.append(socket_map.lookup(bsocket));
      }
    }
  }

  TreeData tree_data(btree);
  for (auto &link : tree_data.data_origins()) {
    if (!inserters.insert_link(builder, link.from, link.to, link.optional_source_link)) {
      return {};
    }
  }

  BSockets unlinked_inputs;
  BNodes unlinked_inputs_nodes;
  SocketVector node_inputs;
  for (bNode *bnode : bNodeList(&btree->nodes)) {
    for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
      if (builder.is_data_socket(bsocket)) {
        Socket socket = socket_map.lookup(bsocket);
        if (!socket.is_linked()) {
          unlinked_inputs.append(bsocket);
          unlinked_inputs_nodes.append(bnode);
          node_inputs.append(socket);
        }
      }
    }
  }

  SocketVector new_origins = inserters.insert_sockets(
      builder, unlinked_inputs, unlinked_inputs_nodes);
  BLI_assert(unlinked_inputs.size() == new_origins.size());

  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    builder.insert_link(new_origins[i], node_inputs[i]);
  }

  auto compact_graph = SharedCompactDataFlowGraph::New(graph.ptr());
  CompactFunctionGraph compact_fgraph();
  FunctionGraph fgraph(graph, input_sockets, output_sockets);
  return fgraph;
}

}  // namespace DataFlowNodes
}  // namespace FN
