#include "DNA_node_types.h"

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
      fn_builder.add_input(vsocket->name(), type);
    }
  }
  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (builder.is_data_socket(vsocket)) {
      SharedType &type = builder.query_socket_type(vsocket);
      fn_builder.add_output(vsocket->name(), type);
    }
  }

  auto fn = fn_builder.build(vnode->name());
  fn->add_body<VNodePlaceholderBody>(vnode);
  BuilderNode *node = builder.insert_function(fn);
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

static bool insert_links(VTreeDataGraphBuilder &builder)
{
  Map<StringPair, ConversionInserter> &map = get_conversion_inserter_map();

  for (VirtualSocket *to_vsocket : builder.vtree().inputs_with_links()) {
    if (to_vsocket->links().size() > 1) {
      continue;
    }
    BLI_assert(to_vsocket->links().size() == 1);
    if (!builder.is_data_socket(to_vsocket)) {
      continue;
    }
    VirtualSocket *from_vsocket = to_vsocket->links()[0];
    if (!builder.is_data_socket(from_vsocket)) {
      return false;
    }

    BuilderOutputSocket *from_socket = builder.lookup_output_socket(from_vsocket);
    BuilderInputSocket *to_socket = builder.lookup_input_socket(to_vsocket);

    if (STREQ(from_vsocket->idname(), to_vsocket->idname())) {
      builder.insert_link(from_socket, to_socket);
      continue;
    }

    StringPair key(from_vsocket->idname(), to_vsocket->idname());
    ConversionInserter *inserter = map.lookup_ptr(key);
    if (inserter != nullptr) {
      (*inserter)(builder, from_socket, to_socket);
    }
    else {
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
    Vector<BuilderInputSocket *> sockets;

    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(vsocket)) {
        BuilderInputSocket *socket = builder.lookup_input_socket(vsocket);
        if (socket->origin() == nullptr) {
          vsockets.append(vsocket);
          sockets.append(socket);
        }
      }
    }

    if (vsockets.size() > 0) {
      Vector<BuilderOutputSocket *> new_origins(vsockets.size());
      unlinked_inputs_handler.insert(builder, vsockets, new_origins);
      builder.insert_links(new_origins, sockets);
    }
  }
}

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree)
{
  VTreeDataGraphBuilder builder(vtree);

  if (!insert_functions_for_bnodes(builder)) {
    return BLI_ERROR_CREATE("error inserting functions for nodes");
  }

  if (!insert_links(builder)) {
    return BLI_ERROR_CREATE("error inserting links");
  }

  ConstantInputsHandler unlinked_inputs_handler;
  insert_unlinked_inputs(builder, unlinked_inputs_handler);

  return builder.build();
}

}  // namespace DataFlowNodes
}  // namespace FN
