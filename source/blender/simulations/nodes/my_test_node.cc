#include <cstring>

#include "BKE_node.h"
#include "SIM_node_tree.h"

#include "BLI_vector.h"
#include "BLI_string_ref.h"
using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class SocketDecl {
 public:
  bNodeTree *m_ntree;
  bNode *m_node;

  virtual void build() const = 0;
};

class InputSocketDecl : public SocketDecl {
};

class InputMockupSocketDecl : public InputSocketDecl {
 public:
  StringRefNull m_ui_name;
  StringRefNull m_identifier;
  StringRefNull m_idname;

  void build() const override
  {
    nodeAddSocket(
        m_ntree, m_node, SOCK_IN, m_idname.data(), m_identifier.data(), m_ui_name.data());
  }
};

class NodeDecl {
 private:
  Vector<SocketDecl *> m_inputs;
  Vector<SocketDecl *> m_outputs;
};

static void init_node(bNodeTree *ntree, bNode *node)
{
  InputMockupSocketDecl decl;
  decl.m_ntree = ntree;
  decl.m_node = node;
  decl.m_ui_name = "Hello World";
  decl.m_identifier = "myid";
  decl.m_idname = "NodeSocketFloat";
  decl.build();
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
