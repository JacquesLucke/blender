#include <cstring>

#include "BKE_node.h"
#include "SIM_node_tree.h"

static void init_node(bNodeTree *ntree, bNode *node)
{
  nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketFloat", "my_identifier", "My Name");
}

void register_node_type_my_test_node()
{
  static bNodeType ntype = {0};
  ntype.minwidth = 20;
  ntype.minheight = 20;
  ntype.maxwidth = 1000;
  ntype.maxheight = 1000;
  ntype.height = 100;
  ntype.width = 140;

  strcpy(ntype.idname, "MyTestNode");
  strcpy(ntype.ui_name, "My Test Node");
  strcpy(ntype.ui_description, "My Test Node Description");
  ntype.type = NODE_CUSTOM;

  ntype.initfunc = init_node;

  nodeRegisterType(&ntype);
}
