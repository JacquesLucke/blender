#include "DNA_node_types.h"

#include "FN_data_flow_nodes.hpp"

#include "mappings.hpp"

namespace FN {
namespace DataFlowNodes {

static bool insert_nodes(VTreeDataGraphBuilder &builder)
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

static bool insert_unlinked_inputs(VTreeDataGraphBuilder &builder,
                                   UnlinkedInputsGrouper &inputs_grouper,
                                   UnlinkedInputsInserter &inputs_inserter)
{
  MultiVector<VirtualSocket *> groups;
  inputs_grouper.group(builder, groups);

  for (uint i = 0; i < groups.size(); i++) {
    ArrayRef<VirtualSocket *> unlinked_inputs = groups[i];
    Vector<BuilderOutputSocket *> new_origins(unlinked_inputs.size());

    inputs_inserter.insert(builder, unlinked_inputs, new_origins);

    for (uint i = 0; i < unlinked_inputs.size(); i++) {
      builder.insert_link(new_origins[i], builder.lookup_input_socket(unlinked_inputs[i]));
    }
  }

  return true;
}

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree)
{
  GroupByNodeUsage inputs_grouper;
  ConstantInputsHandler inputs_inserter;
  return generate_graph(vtree, inputs_grouper, inputs_inserter);
}

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree,
                                            UnlinkedInputsGrouper &inputs_grouper,
                                            UnlinkedInputsInserter &inputs_inserter)
{
  VTreeDataGraphBuilder builder(vtree);

  if (!insert_nodes(builder)) {
    return BLI_ERROR_CREATE("error inserting functions for nodes");
  }

  if (!insert_links(builder)) {
    return BLI_ERROR_CREATE("error inserting links");
  }

  if (!insert_unlinked_inputs(builder, inputs_grouper, inputs_inserter)) {
    return BLI_ERROR_CREATE("error inserting unlinked inputs");
  }

  return builder.build();
}

}  // namespace DataFlowNodes
}  // namespace FN
