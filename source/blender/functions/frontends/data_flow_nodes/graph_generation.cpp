#include "DNA_node_types.h"

#include "FN_types.hpp"
#include "FN_data_flow_nodes.hpp"

#include "inserters.hpp"

namespace FN {
namespace DataFlowNodes {

static void insert_placeholder_node(VTreeDataGraphBuilder &builder, VirtualNode *vnode)
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

static bool insert_functions_for_bnodes(VTreeDataGraphBuilder &builder)
{
  auto inserters = get_node_inserters_map();

  for (VirtualNode *vnode : builder.vtree().nodes()) {
    NodeInserter *inserter = inserters.lookup_ptr(vnode->idname());
    if (inserter) {
      (*inserter)(builder, vnode);
      BLI_assert(builder.verify_data_sockets_mapped(vnode));
      continue;
    }

    if (builder.has_data_socket(vnode)) {
      insert_placeholder_node(builder, vnode);
    }
  }
  return true;
}

static bool insert_links(VTreeDataGraphBuilder &builder, GraphInserters &inserters)
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

static void insert_unlinked_inputs(VTreeDataGraphBuilder &builder,
                                   UnlinkedInputsHandler &unlinked_inputs_handler)
{

  for (VirtualNode *vnode : builder.vtree().nodes()) {
    Vector<VirtualSocket *> vsockets;
    Vector<DFGB_Socket> sockets;

    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(vsocket)) {
        DFGB_Socket socket = builder.lookup_socket(vsocket);
        if (!socket.is_linked()) {
          vsockets.append(vsocket);
          sockets.append(socket);
        }
      }
    }

    if (vsockets.size() > 0) {
      Vector<DFGB_Socket> new_origins(vsockets.size());
      unlinked_inputs_handler.insert(builder, vsockets, new_origins);
      builder.insert_links(new_origins, sockets);
    }
  }
}

class BasicUnlinkedInputsHandler : public UnlinkedInputsHandler {
 private:
  GraphInserters &m_inserters;

 public:
  BasicUnlinkedInputsHandler(GraphInserters &inserters) : m_inserters(inserters)
  {
  }

  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              ArrayRef<DFGB_Socket> r_new_origins) override
  {
    m_inserters.insert_sockets(builder, unlinked_inputs, r_new_origins);
  }
};

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree)
{
  VTreeDataGraphBuilder builder(vtree);
  GraphInserters &inserters = get_standard_inserters();

  if (!insert_functions_for_bnodes(builder)) {
    return BLI_ERROR_CREATE("error inserting functions for nodes");
  }

  if (!insert_links(builder, inserters)) {
    return BLI_ERROR_CREATE("error inserting links");
  }

  BasicUnlinkedInputsHandler unlinked_inputs_handler(inserters);
  insert_unlinked_inputs(builder, unlinked_inputs_handler);

  return builder.build();
}

}  // namespace DataFlowNodes
}  // namespace FN
