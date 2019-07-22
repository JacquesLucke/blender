#pragma once

#include "builder.hpp"
#include <functional>
#include "BLI_optional.hpp"
#include "BLI_string_map.hpp"
#include "FN_tuple_call.hpp"

struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

typedef std::function<void(BTreeGraphBuilder &builder, struct bNode *bnode)> NodeInserter;

typedef std::function<void(PointerRNA *socket_rna_ptr, Tuple &dst, uint index)> SocketLoader;

typedef std::function<void(
    BTreeGraphBuilder &builder, DFGB_Socket from, DFGB_Socket to, struct bNodeLink *source_link)>
    ConversionInserter;

typedef std::function<SharedFunction()> FunctionGetter;

class GraphInserters {
 private:
  StringMap<NodeInserter> m_node_inserters;
  StringMap<SocketLoader> m_socket_loaders;
  SmallMap<std::pair<SharedType, SharedType>, ConversionInserter> m_conversion_inserters;
  StringMap<SharedType> *m_type_by_data_type;

 public:
  GraphInserters();

  void reg_node_inserter(std::string idname, NodeInserter inserter);
  void reg_node_function(std::string idname, FunctionGetter getter);

  void reg_socket_loader(std::string idname, SocketLoader loader);

  void reg_conversion_inserter(StringRef from_type,
                               StringRef to_type,
                               ConversionInserter inserter);

  void reg_conversion_function(StringRef from_type, StringRef to_type, FunctionGetter getter);

  bool insert_node(BTreeGraphBuilder &builder, struct bNode *bnode);

  DFGB_SocketVector insert_sockets(BTreeGraphBuilder &builder,
                                   ArrayRef<struct bNodeSocket *> bsockets);

  bool insert_link(BTreeGraphBuilder &builder,
                   struct bNodeSocket *from_bsocket,
                   struct bNodeSocket *to_bsocket,
                   struct bNodeLink *source_link = nullptr);
};

GraphInserters &get_standard_inserters();

}  // namespace DataFlowNodes
}  // namespace FN
