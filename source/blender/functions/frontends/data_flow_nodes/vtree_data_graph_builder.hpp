#pragma once

#include "FN_core.hpp"
#include "BKE_node_tree.hpp"
#include "BLI_string_map.hpp"

#include "vtree_data_graph.hpp"

struct ID;
struct PointerRNA;

namespace FN {
namespace DataFlowNodes {

using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;

class VTreeDataGraphBuilder {
 private:
  VirtualNodeTree &m_vtree;
  Vector<BuilderSocket *> m_socket_map;
  Vector<SharedType> m_type_by_vsocket;
  StringMap<SharedType> &m_type_by_idname;
  StringMap<SharedType> &m_type_by_data_type;
  StringMap<std::string> &m_data_type_by_idname;
  DataGraphBuilder m_graph_builder;

 public:
  VTreeDataGraphBuilder(VirtualNodeTree &vtree);

  VTreeDataGraph build();

  Vector<BuilderSocket *> &socket_map()
  {
    return m_socket_map;
  }

  /* Insert Function */
  BuilderNode *insert_function(SharedFunction &fn);
  BuilderNode *insert_matching_function(SharedFunction &fn, VirtualNode *vnode);
  BuilderNode *insert_function(SharedFunction &fn, VirtualNode *vnode);

  /* Insert Link */
  void insert_link(BuilderOutputSocket *from, BuilderInputSocket *to);
  void insert_links(ArrayRef<BuilderOutputSocket *> from, ArrayRef<BuilderInputSocket *> to);

  /* Socket Mapping */
  void map_input_socket(BuilderInputSocket *socket, VirtualSocket *vsocket);
  void map_output_socket(BuilderOutputSocket *socket, VirtualSocket *vsocket);
  void map_sockets(BuilderNode *node, VirtualNode *vnode);
  void map_data_sockets(BuilderNode *node, VirtualNode *vnode);

  BuilderInputSocket *lookup_input_socket(VirtualSocket *vsocket);
  BuilderOutputSocket *lookup_output_socket(VirtualSocket *vsocket);
  bool verify_data_sockets_mapped(VirtualNode *vnode) const;

  /* Type Mapping */
  SharedType &type_by_name(StringRef data_type) const;

  /* Query Node Tree */
  VirtualNodeTree &vtree() const;

  /* Query Socket Information */
  bool is_data_socket(VirtualSocket *vsocket) const;
  SharedType &query_socket_type(VirtualSocket *vsocket) const;

  /* Query Node Information */
  SharedType &query_type_property(VirtualNode *vnode, StringRefNull prop_name) const;
  bool has_data_socket(VirtualNode *vnode) const;

  /* Query RNA */
  SharedType &type_from_rna(PointerRNA &rna, StringRefNull prop_name) const;

 private:
  void initialize_type_by_vsocket_map();
  bool check_if_sockets_are_mapped(VirtualNode *vnode, ArrayRef<VirtualSocket *> vsockets) const;
};

}  // namespace DataFlowNodes
}  // namespace FN
