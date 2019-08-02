#include "unlinked_input_groupers.hpp"

namespace FN {
namespace DataFlowNodes {

void SeparateNodeInputs::handle(VTreeDataGraphBuilder &builder, InputInserter &inserter)
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

void SeparateSocketInputs::handle(VTreeDataGraphBuilder &builder, InputInserter &inserter)
{
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(vsocket)) {
        BuilderInputSocket *socket = builder.lookup_input_socket(vsocket);
        if (socket->origin() == nullptr) {
          std::array<BuilderOutputSocket *, 1> new_origin;
          inserter.insert(builder, {vsocket}, new_origin);
          BLI_assert(new_origin[0]);
          builder.insert_link(new_origin[0], socket);
        }
      }
    }
  }
}

void AllInOneSocketInputs::handle(VTreeDataGraphBuilder &builder, InputInserter &inserter)
{
  Vector<VirtualSocket *> unlinked_input_vsockets;
  Vector<BuilderInputSocket *> unlinked_input_sockets;
  for (VirtualNode *vnode : builder.vtree().nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(vsocket)) {
        BuilderInputSocket *socket = builder.lookup_input_socket(vsocket);
        if (socket->origin() == nullptr) {
          unlinked_input_vsockets.append(vsocket);
          unlinked_input_sockets.append(socket);
        }
      }
    }
  }

  Vector<BuilderOutputSocket *> new_origins(unlinked_input_vsockets.size());
  inserter.insert(builder, unlinked_input_vsockets, new_origins);
  builder.insert_links(new_origins, unlinked_input_sockets);
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

static void insert_input_node_for_sockets_with_same_hash(VTreeDataGraphBuilder &builder,
                                                         ArrayRef<uint> hash_per_vsocket,
                                                         InputInserter &inserter)
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
    Vector<BuilderOutputSocket *> new_origins(unlinked_vsockets.size());
    inserter.insert(builder, unlinked_vsockets, new_origins);

    for (uint i = 0; i < unlinked_vsockets.size(); i++) {
      builder.insert_link(new_origins[i], builder.lookup_input_socket(unlinked_vsockets[i]));
    }
  }
}

void GroupByNodeUsage::handle(VTreeDataGraphBuilder &builder, InputInserter &inserter)
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

  insert_input_node_for_sockets_with_same_hash(builder, hash_per_vsocket, inserter);
}

void GroupBySocketUsage::handle(VTreeDataGraphBuilder &builder, InputInserter &inserter)
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

  insert_input_node_for_sockets_with_same_hash(builder, hash_per_vsocket, inserter);
}

}  // namespace DataFlowNodes
}  // namespace FN
