#include "inserters.hpp"
#include "registry.hpp"

#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace FN {
namespace DataFlowNodes {

using StringPair = std::pair<std::string, std::string>;

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

void GraphInserters::reg_node_inserter(std::string idname, NodeInserter inserter)
{
  m_node_inserters.add_new(idname, inserter);
}

void GraphInserters::reg_node_function(std::string idname, FunctionGetter getter)
{
  auto inserter = [getter](BTreeGraphBuilder &builder, bNode *bnode) {
    SharedFunction fn = getter();
    DFGB_Node *node = builder.insert_function(fn, bnode);
    builder.map_sockets(node, bnode);
  };
  this->reg_node_inserter(idname, inserter);
}

void GraphInserters::reg_socket_loader(std::string idname, SocketLoader loader)
{
  m_socket_loaders.add_new(idname, loader);
}

void GraphInserters::reg_conversion_inserter(std::string from_type,
                                             std::string to_type,
                                             ConversionInserter inserter)
{
  auto key = StringPair(from_type, to_type);
  BLI_assert(!m_conversion_inserters.contains(key));
  m_conversion_inserters.add(key, inserter);
}

void GraphInserters::reg_conversion_function(std::string from_type,
                                             std::string to_type,
                                             FunctionGetter getter)
{
  auto inserter = [getter](BTreeGraphBuilder &builder,
                           DFGB_Socket from,
                           DFGB_Socket to,
                           struct bNodeLink *source_link) {
    auto fn = getter();
    DFGB_Node *node;
    if (source_link == NULL) {
      node = builder.insert_function(fn);
    }
    else {
      node = builder.insert_function(fn, source_link);
    }
    builder.insert_link(from, node->input(0));
    builder.insert_link(node->output(0), to);
  };
  this->reg_conversion_inserter(from_type, to_type, inserter);
}

bool GraphInserters::insert_node(BTreeGraphBuilder &builder, bNode *bnode)
{
  NodeInserter *inserter = m_node_inserters.lookup_ptr(bnode->idname);
  if (inserter == nullptr) {
    return false;
  }
  (*inserter)(builder, bnode);

  BLI_assert(builder.verify_data_sockets_mapped(bnode));
  return true;
}

class SocketLoaderBody : public TupleCallBody {
 private:
  bNodeTree *m_btree;
  SmallVector<bNodeSocket *> m_bsockets;
  SmallVector<SocketLoader> m_loaders;

 public:
  SocketLoaderBody(bNodeTree *btree,
                   ArrayRef<bNodeSocket *> bsockets,
                   SmallVector<SocketLoader> &loaders)
      : m_btree(btree), m_bsockets(bsockets), m_loaders(loaders)
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    for (uint i = 0; i < m_bsockets.size(); i++) {
      PointerRNA rna;
      bNodeSocket *bsocket = m_bsockets[i];
      auto loader = m_loaders[i];

      RNA_pointer_create(&m_btree->id, &RNA_NodeSocket, bsocket, &rna);
      loader(&rna, fn_out, i);
    }
  }
};

class SocketLoaderDependencies : public DepsBody {
 private:
  bNodeTree *m_btree;
  SmallVector<bNodeSocket *> m_bsockets;

 public:
  SocketLoaderDependencies(bNodeTree *btree, ArrayRef<bNodeSocket *> bsockets)
      : m_btree(btree), m_bsockets(bsockets)
  {
  }

  void build_deps(FunctionDepsBuilder &builder) const
  {
    for (uint i = 0; i < m_bsockets.size(); i++) {
      bNodeSocket *bsocket = m_bsockets[i];
      if (STREQ(bsocket->idname, "fn_ObjectSocket")) {
        PointerRNA rna;
        RNA_pointer_create(&m_btree->id, &RNA_NodeSocket, bsocket, &rna);
        Object *value = (Object *)RNA_pointer_get(&rna, "value").id.data;
        if (value != nullptr) {
          builder.add_output_objects(i, {value});
        }
      }
    }
  }
};

DFGB_SocketVector GraphInserters::insert_sockets(BTreeGraphBuilder &builder,
                                                 ArrayRef<bNodeSocket *> bsockets)
{
  SmallVector<SocketLoader> loaders;

  FunctionBuilder fn_builder;
  for (uint i = 0; i < bsockets.size(); i++) {
    bNodeSocket *bsocket = bsockets[i];

    PointerRNA rna = builder.get_rna(bsocket);

    char data_type[64];
    RNA_string_get(&rna, "data_type", data_type);

    SocketLoader loader = m_socket_loaders.lookup(data_type);
    loaders.append(loader);
    fn_builder.add_output(builder.query_socket_name(bsocket), builder.query_socket_type(bsocket));
  }

  auto fn = fn_builder.build("Input Sockets");
  fn->add_body<SocketLoaderBody>(builder.btree(), bsockets, loaders);
  fn->add_body<SocketLoaderDependencies>(builder.btree(), bsockets);
  DFGB_Node *node = builder.insert_function(fn);

  DFGB_SocketVector sockets;
  for (DFGB_Socket output : node->outputs()) {
    sockets.append(output);
  }
  return sockets;
}

bool GraphInserters::insert_link(BTreeGraphBuilder &builder,
                                 struct bNodeSocket *from_bsocket,
                                 struct bNodeSocket *to_bsocket,
                                 struct bNodeLink *source_link)
{
  BLI_assert(builder.is_data_socket(from_bsocket));
  BLI_assert(builder.is_data_socket(to_bsocket));

  DFGB_Socket from_socket = builder.lookup_socket(from_bsocket);
  DFGB_Socket to_socket = builder.lookup_socket(to_bsocket);

  std::string from_type = builder.query_socket_type_name(from_bsocket);
  std::string to_type = builder.query_socket_type_name(to_bsocket);

  if (from_type == to_type) {
    builder.insert_link(from_socket, to_socket);
    return true;
  }

  auto key = StringPair(from_type, to_type);
  if (m_conversion_inserters.contains(key)) {
    auto inserter = m_conversion_inserters.lookup(key);
    inserter(builder, from_socket, to_socket, source_link);
    return true;
  }

  return false;
}

}  // namespace DataFlowNodes
}  // namespace FN

namespace std {
template<> struct hash<FN::DataFlowNodes::StringPair> {
  typedef FN::DataFlowNodes::StringPair argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    size_t h1 = std::hash<std::string>{}(v.first);
    size_t h2 = std::hash<std::string>{}(v.second);
    return h1 ^ h2;
  }
};
}  // namespace std
