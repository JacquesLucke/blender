#include "DNA_node_types.h"

#include "FN_types.hpp"
#include "FN_dependencies.hpp"
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

class SocketLoaderBody : public TupleCallBody {
 private:
  Vector<bNodeTree *> m_btrees;
  Vector<bNodeSocket *> m_bsockets;
  Vector<SocketLoader> m_loaders;

 public:
  SocketLoaderBody(ArrayRef<bNodeTree *> btrees,
                   ArrayRef<bNodeSocket *> bsockets,
                   Vector<SocketLoader> &loaders)
      : m_btrees(btrees), m_bsockets(bsockets), m_loaders(loaders)
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    for (uint i = 0; i < m_bsockets.size(); i++) {
      PointerRNA rna;
      bNodeSocket *bsocket = m_bsockets[i];
      auto loader = m_loaders[i];
      bNodeTree *btree = m_btrees[i];

      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &rna);
      loader(&rna, fn_out, i);
    }
  }
};

class SocketLoaderDependencies : public DepsBody {
 private:
  Vector<bNodeTree *> m_btrees;
  Vector<bNodeSocket *> m_bsockets;

 public:
  SocketLoaderDependencies(ArrayRef<bNodeTree *> btrees, ArrayRef<bNodeSocket *> bsockets)
      : m_btrees(btrees), m_bsockets(bsockets)
  {
  }

  void build_deps(FunctionDepsBuilder &builder) const
  {
    for (uint i = 0; i < m_bsockets.size(); i++) {
      bNodeSocket *bsocket = m_bsockets[i];
      bNodeTree *btree = m_btrees[i];
      if (STREQ(bsocket->idname, "fn_ObjectSocket")) {
        PointerRNA rna;
        RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &rna);
        Object *value = (Object *)RNA_pointer_get(&rna, "value").id.data;
        if (value != nullptr) {
          builder.add_output_objects(i, {value});
        }
      }
    }
  }
};

class BasicUnlinkedInputsHandler : public UnlinkedInputsHandler {
 public:
  BasicUnlinkedInputsHandler() = default;

  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              ArrayRef<BuilderOutputSocket *> r_new_origins) override
  {
    auto &socket_loader_map = get_socket_loader_map();

    Vector<SocketLoader> loaders;
    Vector<bNodeSocket *> bsockets;
    Vector<bNodeTree *> btrees;

    FunctionBuilder fn_builder;
    for (uint i = 0; i < unlinked_inputs.size(); i++) {
      VirtualSocket *vsocket = unlinked_inputs[i];

      SocketLoader loader = socket_loader_map.lookup(vsocket->bsocket()->idname);
      loaders.append(loader);
      fn_builder.add_output(vsocket->name(), builder.query_socket_type(vsocket));

      bsockets.append(vsocket->bsocket());
      btrees.append(vsocket->btree());
    }

    auto fn = fn_builder.build("Input Sockets");
    fn->add_body<SocketLoaderBody>(btrees, bsockets, loaders);
    fn->add_body<SocketLoaderDependencies>(btrees, bsockets);
    BuilderNode *node = builder.insert_function(fn);

    for (uint i = 0; i < node->outputs().size(); i++) {
      r_new_origins[i] = node->output(i);
    }
  }
};

ValueOrError<VTreeDataGraph> generate_graph(VirtualNodeTree &vtree)
{
  VTreeDataGraphBuilder builder(vtree);

  if (!insert_functions_for_bnodes(builder)) {
    return BLI_ERROR_CREATE("error inserting functions for nodes");
  }

  if (!insert_links(builder)) {
    return BLI_ERROR_CREATE("error inserting links");
  }

  BasicUnlinkedInputsHandler unlinked_inputs_handler;
  insert_unlinked_inputs(builder, unlinked_inputs_handler);

  return builder.build();
}

}  // namespace DataFlowNodes
}  // namespace FN
