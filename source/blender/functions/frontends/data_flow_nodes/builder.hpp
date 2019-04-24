#pragma once

#include "FN_core.hpp"
#include "./util_wrappers.hpp"

struct bNode;
struct bNodeLink;
struct bNodeTree;
struct bNodeSocket;
struct ID;
struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

using SocketMap = SmallMap<struct bNodeSocket *, Socket>;

class GraphBuilder {
 private:
  struct bNodeTree *m_btree;
  DataFlowGraph *m_graph;
  SocketMap &m_socket_map;

 public:
  GraphBuilder(struct bNodeTree *btree, DataFlowGraph *graph, SocketMap &socket_map)
      : m_btree(btree), m_graph(graph), m_socket_map(socket_map)
  {
  }

  /* Insert Function */
  Node *insert_function(SharedFunction &fn);
  Node *insert_matching_function(SharedFunction &fn, struct bNode *bnode);
  Node *insert_function(SharedFunction &fn, struct bNode *bnode);
  Node *insert_function(SharedFunction &fn, struct bNodeLink *blink);

  /* Insert Link */
  void insert_link(Socket a, Socket b);

  /* Socket Mapping */
  void map_socket(Socket socket, struct bNodeSocket *bsocket);
  void map_sockets(Node *node, struct bNode *bnode);
  void map_data_sockets(Node *node, struct bNode *bnode);
  void map_input(Socket socket, struct bNode *bnode, uint index);
  void map_output(Socket socket, struct bNode *bnode, uint index);

  Socket lookup_socket(struct bNodeSocket *bsocket);
  bool verify_data_sockets_mapped(struct bNode *bnode) const;

  /* Type Mapping */
  SharedType &type_by_name(const char *data_type) const;

  /* Query Node Tree */
  bNodeTree *btree() const;
  ID *btree_id() const;

  /* Query Socket Information */
  PointerRNA get_rna(bNodeSocket *bsocket) const;
  bool is_data_socket(bNodeSocket *bsocket) const;
  std::string query_socket_name(bNodeSocket *bsocket) const;
  SharedType &query_socket_type(bNodeSocket *bsocket) const;
  std::string query_socket_type_name(bNodeSocket *bsocket) const;

  /* Query Node Information */
  PointerRNA get_rna(bNode *bnode) const;
  SharedType &query_type_property(bNode *bnode, const char *prop_name) const;

  /* Query RNA */
  SharedType &type_from_rna(PointerRNA &rna, const char *prop_name) const;

 private:
  bool check_if_sockets_are_mapped(struct bNode *bnode, bSocketList bsockets) const;
};

}  // namespace DataFlowNodes
}  // namespace FN
