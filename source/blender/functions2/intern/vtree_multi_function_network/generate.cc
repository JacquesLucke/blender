#include "FN_vtree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"

#include "mappings.h"
#include "builder.h"

namespace FN {

static bool insert_nodes(VTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  const VirtualNodeTree &vtree = builder.vtree();

  for (const VNode *vnode : vtree.nodes()) {
    StringRef idname = vnode->idname();
    const InsertVNodeFunction *inserter = mappings.vnode_inserters.lookup_ptr(idname);

    if (inserter != nullptr) {
      (*inserter)(builder, *vnode);
      BLI_assert(builder.data_sockets_of_vnode_are_mapped(*vnode));
    }
    else if (builder.has_data_sockets(*vnode)) {
      builder.add_dummy(*vnode);
    }
  }

  return true;
}

static bool insert_links(VTreeMFNetworkBuilder &builder,
                         const VTreeMultiFunctionMappings &mappings)
{
  for (const VInputSocket *to_vsocket : builder.vtree().all_input_sockets()) {
    ArrayRef<const VOutputSocket *> origins = to_vsocket->linked_sockets();
    if (origins.size() != 1) {
      continue;
    }

    if (!builder.is_data_socket(*to_vsocket)) {
      continue;
    }

    const VOutputSocket *from_vsocket = origins[0];
    if (!builder.is_data_socket(*from_vsocket)) {
      return false;
    }

    MFBuilderOutputSocket &from_socket = builder.lookup_socket(*from_vsocket);
    MFBuilderInputSocket &to_socket = builder.lookup_socket(*to_vsocket);

    if (from_socket.type() == to_socket.type()) {
      builder.add_link(from_socket, to_socket);
    }
    else {
      const InsertImplicitConversionFunction *inserter = mappings.conversion_inserters.lookup_ptr(
          {from_vsocket->idname(), to_vsocket->idname()});
      if (inserter == nullptr) {
        return false;
      }
      auto new_sockets = (*inserter)(builder);
      builder.add_link(from_socket, *new_sockets.first);
      builder.add_link(*new_sockets.second, to_socket);
    }
  }

  return true;
}

static bool insert_unlinked_inputs(VTreeMFNetworkBuilder &builder,
                                   const VTreeMultiFunctionMappings &mappings)
{
  Vector<const VInputSocket *> unlinked_data_inputs;
  for (const VInputSocket *vsocket : builder.vtree().all_input_sockets()) {
    if (builder.is_data_socket(*vsocket)) {
      if (!builder.is_input_linked(*vsocket)) {
        unlinked_data_inputs.append(vsocket);
      }
    }
  }

  for (const VInputSocket *vsocket : unlinked_data_inputs) {
    const InsertUnlinkedInputFunction *inserter = mappings.input_inserters.lookup_ptr(
        vsocket->idname());

    if (inserter == nullptr) {
      return false;
    }
    MFBuilderOutputSocket &from_socket = (*inserter)(builder, *vsocket);
    MFBuilderInputSocket &to_socket = builder.lookup_socket(*vsocket);
    builder.add_link(from_socket, to_socket);
  }

  return true;
}

std::unique_ptr<VTreeMFNetwork> generate_vtree_multi_function_network(const VirtualNodeTree &vtree,
                                                                      OwnedResources &resources)
{
  const VTreeMultiFunctionMappings &mappings = get_vtree_multi_function_mappings();

  VTreeMFNetworkBuilder builder(vtree, mappings, resources);
  if (!insert_nodes(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_links(builder, mappings)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(builder, mappings)) {
    BLI_assert(false);
  }

  auto vtree_network = builder.build();
  return vtree_network;
}

std::unique_ptr<MF_EvaluateNetwork> generate_vtree_multi_function(const VirtualNodeTree &vtree,
                                                                  OwnedResources &resources)
{
  std::unique_ptr<VTreeMFNetwork> network = generate_vtree_multi_function_network(vtree,
                                                                                  resources);

  auto input_vnodes = vtree.nodes_with_idname("fn_FunctionInputNode");
  auto output_vnodes = vtree.nodes_with_idname("fn_FunctionOutputNode");

  Vector<const MFOutputSocket *> function_inputs;
  Vector<const MFInputSocket *> function_outputs;

  if (input_vnodes.size() == 1) {
    auto vsockets = input_vnodes.first()->outputs().drop_back(1);
    function_inputs.append_n_times(nullptr, vsockets.size());
    network->lookup_sockets(vsockets, function_inputs);
  }

  if (output_vnodes.size() == 1) {
    auto vsockets = output_vnodes.first()->inputs().drop_back(1);
    function_outputs.append_n_times(nullptr, vsockets.size());
    network->lookup_sockets(vsockets, function_outputs);
  }

  auto function = BLI::make_unique<MF_EvaluateNetwork>(std::move(function_inputs),
                                                       std::move(function_outputs));
  resources.add(std::move(network), "VTree Multi Function Network");
  return function;
}

}  // namespace FN
