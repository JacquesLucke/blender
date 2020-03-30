#include "BLI_listbase.h"
#include "node_fn_util.h"

static bNodeSocketTemplate fn_node_switch_in[] = {
    {SOCK_BOOLEAN, N_("Switch")},

    {SOCK_FLOAT, N_("If False"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_INT, N_("If False"), 0, 0, 0, 0, -10000, 10000},
    {SOCK_BOOLEAN, N_("If False")},
    {SOCK_VECTOR, N_("If False"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_STRING, N_("If False")},
    {SOCK_RGBA, N_("If False"), 0.8f, 0.8f, 0.8f, 1.0f},
    {SOCK_OBJECT, N_("If False")},
    {SOCK_IMAGE, N_("If False")},
    {SOCK_SURFACE_HOOK, N_("If False")},

    {SOCK_FLOAT, N_("If True"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_INT, N_("If True"), 0, 0, 0, 0, -10000, 10000},
    {SOCK_BOOLEAN, N_("If True")},
    {SOCK_VECTOR, N_("If True"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {SOCK_STRING, N_("If True")},
    {SOCK_RGBA, N_("If True"), 0.8f, 0.8f, 0.8f, 1.0f},
    {SOCK_OBJECT, N_("If True")},
    {SOCK_IMAGE, N_("If True")},
    {SOCK_SURFACE_HOOK, N_("If True")},

    {-1, ""},
};

static bNodeSocketTemplate fn_node_switch_out[] = {
    {SOCK_FLOAT, N_("Result")},
    {SOCK_INT, N_("Result")},
    {SOCK_BOOLEAN, N_("Result")},
    {SOCK_VECTOR, N_("Result")},
    {SOCK_STRING, N_("Result")},
    {SOCK_RGBA, N_("Result")},
    {SOCK_OBJECT, N_("Result")},
    {SOCK_IMAGE, N_("Result")},
    {SOCK_SURFACE_HOOK, N_("Result")},
    {-1, ""},
};

static void fn_node_switch_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  int index = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    nodeSetSocketAvailability(sock, index == 0 || sock->type == node->custom1);
    index++;
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    nodeSetSocketAvailability(sock, sock->type == node->custom1);
  }
}

void register_node_type_fn_switch()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SWITCH, "Switch", 0, 0);
  node_type_socket_templates(&ntype, fn_node_switch_in, fn_node_switch_out);
  node_type_update(&ntype, fn_node_switch_update);
  nodeRegisterType(&ntype);
}
