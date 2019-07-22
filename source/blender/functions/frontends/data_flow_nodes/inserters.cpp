#include "inserters.hpp"
#include "registry.hpp"
#include "type_mappings.hpp"

#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace FN {
namespace DataFlowNodes {

using TypePair = std::pair<SharedType, SharedType>;

static void initialize_standard_inserters(GraphInserters &inserters)
{
  register_node_inserters(inserters);
  initialize_socket_inserters(inserters);
  register_conversion_inserters(inserters);
}

BLI_LAZY_INIT(GraphInserters, get_standard_inserters)
{
  GraphInserters inserters;
  initialize_standard_inserters(inserters);
  return inserters;
}

GraphInserters::GraphInserters()
    : m_type_by_data_type(&get_type_by_data_type_map()),
      m_type_by_idname(&get_type_by_idname_map())
{
}

void GraphInserters::reg_node_inserter(std::string idname, NodeInserter inserter)
{
  m_node_inserters.add_new(idname, inserter);
}

void GraphInserters::reg_node_function(std::string idname, FunctionGetter getter)
{
  auto inserter = [getter](BTreeGraphBuilder &builder, VirtualNode *vnode) {
    SharedFunction fn = getter();
    DFGB_Node *node = builder.insert_function(fn, vnode);
    builder.map_sockets(node, vnode);
  };
  this->reg_node_inserter(idname, inserter);
}

void GraphInserters::reg_socket_loader(std::string idname, SocketLoader loader)
{
  m_socket_loaders.add_new(idname, loader);
}

void GraphInserters::reg_conversion_inserter(StringRef from_type,
                                             StringRef to_type,
                                             ConversionInserter inserter)
{
  auto key = TypePair(m_type_by_data_type->lookup(from_type),
                      m_type_by_data_type->lookup(to_type));
  BLI_assert(!m_conversion_inserters.contains(key));
  m_conversion_inserters.add(key, inserter);
}

void GraphInserters::reg_conversion_function(StringRef from_type,
                                             StringRef to_type,
                                             FunctionGetter getter)
{
  auto inserter = [getter](BTreeGraphBuilder &builder, DFGB_Socket from, DFGB_Socket to) {
    auto fn = getter();
    DFGB_Node *node = builder.insert_function(fn);
    builder.insert_link(from, node->input(0));
    builder.insert_link(node->output(0), to);
  };
  this->reg_conversion_inserter(from_type, to_type, inserter);
}

bool GraphInserters::insert_node(BTreeGraphBuilder &builder, VirtualNode *vnode)
{
  NodeInserter *inserter = m_node_inserters.lookup_ptr(vnode->bnode()->idname);
  if (inserter == nullptr) {
    return false;
  }
  (*inserter)(builder, vnode);

  BLI_assert(builder.verify_data_sockets_mapped(vnode));
  return true;
}

class SocketLoaderBody : public TupleCallBody {
 private:
  SmallVector<bNodeTree *> m_btrees;
  SmallVector<bNodeSocket *> m_bsockets;
  SmallVector<SocketLoader> m_loaders;

 public:
  SocketLoaderBody(ArrayRef<bNodeTree *> btrees,
                   ArrayRef<bNodeSocket *> bsockets,
                   SmallVector<SocketLoader> &loaders)
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
  SmallVector<bNodeTree *> m_btrees;
  SmallVector<bNodeSocket *> m_bsockets;

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

DFGB_SocketVector GraphInserters::insert_sockets(BTreeGraphBuilder &builder,
                                                 ArrayRef<VirtualSocket *> vsockets)
{
  SmallVector<SocketLoader> loaders;
  SmallVector<bNodeSocket *> bsockets;
  SmallVector<bNodeTree *> btrees;

  FunctionBuilder fn_builder;
  for (uint i = 0; i < vsockets.size(); i++) {
    VirtualSocket *vsocket = vsockets[i];

    SocketLoader loader = m_socket_loaders.lookup(vsocket->bsocket()->idname);
    loaders.append(loader);
    fn_builder.add_output(builder.query_socket_name(vsocket), builder.query_socket_type(vsocket));

    bsockets.append(vsocket->bsocket());
    btrees.append(vsocket->btree());
  }

  auto fn = fn_builder.build("Input Sockets");
  fn->add_body<SocketLoaderBody>(btrees, bsockets, loaders);
  fn->add_body<SocketLoaderDependencies>(btrees, bsockets);
  DFGB_Node *node = builder.insert_function(fn);

  DFGB_SocketVector sockets;
  for (DFGB_Socket output : node->outputs()) {
    sockets.append(output);
  }
  return sockets;
}

bool GraphInserters::insert_link(BTreeGraphBuilder &builder,
                                 VirtualSocket *from_vsocket,
                                 VirtualSocket *to_vsocket)
{
  BLI_assert(builder.is_data_socket(from_vsocket));
  BLI_assert(builder.is_data_socket(to_vsocket));

  DFGB_Socket from_socket = builder.lookup_socket(from_vsocket);
  DFGB_Socket to_socket = builder.lookup_socket(to_vsocket);

  SharedType &from_type = builder.query_socket_type(from_vsocket);
  SharedType &to_type = builder.query_socket_type(to_vsocket);

  if (from_type == to_type) {
    builder.insert_link(from_socket, to_socket);
    return true;
  }

  auto key = TypePair(from_type, to_type);
  if (m_conversion_inserters.contains(key)) {
    auto inserter = m_conversion_inserters.lookup(key);
    inserter(builder, from_socket, to_socket);
    return true;
  }

  return false;
}

}  // namespace DataFlowNodes
}  // namespace FN

namespace std {
template<> struct hash<FN::DataFlowNodes::TypePair> {
  typedef FN::DataFlowNodes::TypePair argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    size_t h1 = std::hash<FN::SharedType>{}(v.first);
    size_t h2 = std::hash<FN::SharedType>{}(v.second);
    return h1 ^ h2;
  }
};
}  // namespace std
