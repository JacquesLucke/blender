#include "DNA_node_types.h"

#include "FN_data_flow_nodes.hpp"

#include "mappings.hpp"

namespace FN {
namespace DataFlowNodes {

static bool insert_functions_for_bnodes(VTreeDataGraphBuilder &builder)
{
  auto &inserters = MAPPING_node_inserters();

  for (VirtualNode *vnode : builder.vtree().nodes()) {
    if (inserters->insert(builder, vnode)) {
      BLI_assert(builder.verify_data_sockets_mapped(vnode));
      continue;
    }

    if (builder.has_data_socket(vnode)) {
      builder.insert_placeholder(vnode);
    }
  }
  return true;
}

static bool insert_links(VTreeDataGraphBuilder &builder)
{
  std::unique_ptr<LinkInserters> &inserters = MAPPING_link_inserters();

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

    if (!inserters->insert(builder, from_vsocket, to_vsocket)) {
      return false;
    }
  }
  return true;
}

class SeparateNodeInputs : public UnlinkedInputsHandler {
 public:
  void handle(VTreeDataGraphBuilder &builder, InputInserter &inserter) override
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
        inserter.insert(builder, vsockets, new_origins);
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

  ConstantInputsHandler input_inserter;
  insert_unlinked_inputs(builder, input_inserter);

  return builder.build();
}

}  // namespace DataFlowNodes
}  // namespace FN
