#include "FN_multi_function_builder.hh"

#include "BKE_node_tree_multi_function.hh"

#include "node_function_util.h"

namespace blender {
namespace node {

static bNodeSocketTemplate fn_node_combine_strings_in[] = {
    {SOCK_STRING, N_("A")},
    {SOCK_STRING, N_("B")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_combine_strings_out[] = {
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static void fn_node_combine_strings_expand_in_mf_network(bke::NodeMFNetworkBuilder &builder)
{
  static fn::CustomMF_SI_SI_SO<std::string, std::string, std::string> combine_fn{
      "Combine Strings", [](const std::string &a, const std::string &b) { return a + b; }};
  builder.set_matching_fn(combine_fn);
}

}  // namespace node
}  // namespace blender

void register_node_type_fn_combine_strings()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_STRINGS, "Combine Strings", 0, 0);
  node_type_socket_templates(&ntype,
                             blender::node::fn_node_combine_strings_in,
                             blender::node::fn_node_combine_strings_out);
  ntype.expand_in_mf_network = blender::node::fn_node_combine_strings_expand_in_mf_network;
  nodeRegisterType(&ntype);
}
