#include "node_function_util.hh"

namespace blender {
namespace node {

static bNodeSocketTemplate fn_node_group_instance_id_out[] = {
    {SOCK_STRING, N_("Identifier")},
    {-1, ""},
};

static void fn_node_group_instance_id_expand_in_mf_network(bke::NodeMFNetworkBuilder &builder)
{
  const bke::DNode &node = builder.dnode();
  std::string id = "/";
  for (const bke::DParentNode *parent = node.parent(); parent; parent = parent->parent()) {
    id = "/" + parent->node_ref().name() + id;
  }
  builder.construct_and_set_matching_fn<fn::CustomMF_Constant<std::string>>(std::move(id));
}

}  // namespace node
}  // namespace blender

void register_node_type_fn_group_instance_id()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_GROUP_INSTANCE_ID, "Group Instance ID", 0, 0);
  node_type_socket_templates(&ntype, nullptr, blender::node::fn_node_group_instance_id_out);
  ntype.expand_in_mf_network = blender::node::fn_node_group_instance_id_expand_in_mf_network;
  nodeRegisterType(&ntype);
}
