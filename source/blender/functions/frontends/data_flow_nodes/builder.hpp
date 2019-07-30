#pragma once

#include "FN_core.hpp"
#include "BKE_node_tree.hpp"
#include "BLI_string_map.hpp"

struct ID;
struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;

class BTreeGraphBuilder {
 private:
  DataFlowGraphBuilder &m_graph;
  VirtualNodeTree &m_vtree;
  Map<VirtualSocket *, DFGB_Socket> &m_socket_map;
  StringMap<SharedType> &m_type_by_idname;
  StringMap<SharedType> &m_type_by_data_type;

 public:
  BTreeGraphBuilder(VirtualNodeTree &vtree,
                    DataFlowGraphBuilder &graph,
                    Map<VirtualSocket *, DFGB_Socket> &socket_map);

  /* Insert Function */
  DFGB_Node *insert_function(SharedFunction &fn);
  DFGB_Node *insert_matching_function(SharedFunction &fn, VirtualNode *vnode);
  DFGB_Node *insert_function(SharedFunction &fn, VirtualNode *vnode);

  /* Insert Link */
  void insert_link(DFGB_Socket a, DFGB_Socket b);
  void insert_links(ArrayRef<DFGB_Socket> a, ArrayRef<DFGB_Socket> b);

  /* Socket Mapping */
  void map_socket(DFGB_Socket socket, VirtualSocket *vsocket);
  void map_sockets(DFGB_Node *node, VirtualNode *vnode);
  void map_data_sockets(DFGB_Node *node, VirtualNode *vnode);
  void map_input(DFGB_Socket socket, VirtualNode *vnode, uint index);
  void map_output(DFGB_Socket socket, VirtualNode *vnode, uint index);

  DFGB_Socket lookup_socket(VirtualSocket *vsocket);
  bool verify_data_sockets_mapped(VirtualNode *vnode) const;

  /* Type Mapping */
  SharedType &type_by_name(StringRef data_type) const;

  /* Query Node Tree */
  VirtualNodeTree &vtree() const;

  /* Query Socket Information */
  bool is_data_socket(VirtualSocket *vsocket) const;
  std::string query_socket_name(VirtualSocket *vsocket) const;
  SharedType &query_socket_type(VirtualSocket *vsocket) const;

  /* Query Node Information */
  SharedType &query_type_property(VirtualNode *vnode, StringRefNull prop_name) const;
  bool has_data_socket(VirtualNode *vnode) const;

  /* Query RNA */
  SharedType &type_from_rna(PointerRNA &rna, StringRefNull prop_name) const;

 private:
  bool check_if_sockets_are_mapped(VirtualNode *vnode, ArrayRef<VirtualSocket *> vsockets) const;
};

}  // namespace DataFlowNodes
}  // namespace FN
