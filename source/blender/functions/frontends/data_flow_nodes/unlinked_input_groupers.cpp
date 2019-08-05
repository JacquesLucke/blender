#include "unlinked_input_groupers.hpp"

namespace FN {
namespace DataFlowNodes {

void SeparateNodeInputs::group(VTreeDataGraphBuilder &builder,
                               MultiVector<VirtualSocket *> &r_groups)
{
  Vector<VirtualSocket *> vsockets;
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_input_unlinked(vsocket)) {
        vsockets.append(vsocket);
      }
    }

    if (vsockets.size() > 0) {
      r_groups.append(vsockets);
      vsockets.clear();
    }
  }
}

void SeparateSocketInputs::group(VTreeDataGraphBuilder &builder,
                                 MultiVector<VirtualSocket *> &r_groups)
{
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_input_unlinked(vsocket)) {
        r_groups.append({vsocket});
      }
    }
  }
}

void AllInOneSocketInputs::group(VTreeDataGraphBuilder &builder,
                                 MultiVector<VirtualSocket *> &r_groups)
{
  Vector<VirtualSocket *> unlinked_input_vsockets;
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_input_unlinked(vsocket)) {
        unlinked_input_vsockets.append(vsocket);
      }
    }
  }

  r_groups.append(unlinked_input_vsockets);
}

static void update_hash_of_used_vsockets(VTreeDataGraphBuilder &builder,
                                         VirtualSocket *vsocket,
                                         uint random,
                                         ArrayRef<uint> hash_per_vsocket,
                                         ArrayRef<bool> was_updated_per_vsocket,
                                         Vector<uint> &updated_vsockets)
{
  hash_per_vsocket[vsocket->id()] ^= random;
  was_updated_per_vsocket[vsocket->id()] = true;
  updated_vsockets.append(vsocket->id());

  if (vsocket->is_input()) {
    for (VirtualSocket *origin : vsocket->links()) {
      if (builder.is_data_socket(origin) && !was_updated_per_vsocket[origin->id()]) {
        update_hash_of_used_vsockets(
            builder, origin, random, hash_per_vsocket, was_updated_per_vsocket, updated_vsockets);
      }
    }
  }
  else {
    for (VirtualSocket *input : vsocket->vnode()->inputs()) {
      if (builder.is_data_socket(input) && !was_updated_per_vsocket[input->id()]) {
        update_hash_of_used_vsockets(
            builder, input, random, hash_per_vsocket, was_updated_per_vsocket, updated_vsockets);
      }
    }
  }
}

static void group_with_same_hash(VTreeDataGraphBuilder &builder,
                                 ArrayRef<uint> hash_per_vsocket,
                                 MultiVector<VirtualSocket *> &r_groups)
{
  MultiMap<uint, VirtualSocket *> unlinked_inputs_by_hash;
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(vsocket)) {
        BuilderInputSocket *socket = builder.lookup_input_socket(vsocket);
        if (socket->origin() == nullptr) {
          unlinked_inputs_by_hash.add(hash_per_vsocket[vsocket->id()], vsocket);
        }
      }
    }
  }

  /* TODO(jacques): replace with values iterator when it exists. */
  for (uint key : unlinked_inputs_by_hash.keys()) {
    ArrayRef<VirtualSocket *> unlinked_vsockets = unlinked_inputs_by_hash.lookup(key);
    BLI_assert(unlinked_vsockets.size() > 0);
    r_groups.append(unlinked_vsockets);
  }
}

void GroupByNodeUsage::group(VTreeDataGraphBuilder &builder,
                             MultiVector<VirtualSocket *> &r_groups)
{
  uint socket_count = builder.vtree().socket_count();

  Vector<uint> hash_per_vsocket(socket_count, 0);
  Vector<bool> was_updated_per_vsocket(socket_count, false);
  Vector<uint> updated_vsockets;

  for (BuilderNode *node : builder.placeholder_nodes()) {
    VNodePlaceholderBody &placeholder_info = node->function()->body<VNodePlaceholderBody>();
    uint random = rand();
    for (VirtualSocket *vsocket : placeholder_info.inputs()) {
      update_hash_of_used_vsockets(
          builder, vsocket, random, hash_per_vsocket, was_updated_per_vsocket, updated_vsockets);
    }
    was_updated_per_vsocket.fill_indices(updated_vsockets, false);
    updated_vsockets.clear();
  }

  group_with_same_hash(builder, hash_per_vsocket, r_groups);
}

void GroupBySocketUsage::group(VTreeDataGraphBuilder &builder,
                               MultiVector<VirtualSocket *> &r_groups)
{
  uint socket_count = builder.vtree().socket_count();

  Vector<uint> hash_per_vsocket(socket_count, 0);
  Vector<bool> was_updated_per_vsocket(socket_count, false);
  Vector<uint> updated_vsockets;

  for (BuilderNode *node : builder.placeholder_nodes()) {
    VNodePlaceholderBody &placeholder_info = node->function()->body<VNodePlaceholderBody>();
    for (VirtualSocket *vsocket : placeholder_info.inputs()) {
      update_hash_of_used_vsockets(
          builder, vsocket, rand(), hash_per_vsocket, was_updated_per_vsocket, updated_vsockets);
      was_updated_per_vsocket.fill_indices(updated_vsockets, false);
      updated_vsockets.clear();
    }
  }

  group_with_same_hash(builder, hash_per_vsocket, r_groups);
}

}  // namespace DataFlowNodes
}  // namespace FN
