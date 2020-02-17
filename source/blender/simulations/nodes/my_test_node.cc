#include <cstring>

#include "BKE_node.h"
#include "SIM_node_tree.h"

#include "BLI_vector.h"
#include "BLI_string_ref.h"
#include "BLI_set.h"

using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class SocketDataType;
class BaseSocketDataType;
class ListSocketDataType;

enum class SocketTypeCategory {
  Base,
  List,
};

class SocketDataType {
 public:
  using BuildFunc = std::function<bNodeSocket *(
      bNode *node, StringRef name, StringRef identifer, eNodeSocketInOut in_out)>;

  std::string m_ui_name;
  BuildFunc m_build_fn;
  SocketTypeCategory m_category;

  SocketDataType(StringRef ui_name, BuildFunc build_fn, SocketTypeCategory category)
      : m_ui_name(ui_name), m_build_fn(std::move(build_fn)), m_category(category)
  {
  }
};

class BaseSocketDataType : public SocketDataType {
 public:
  ListSocketDataType *m_list_type;

  BaseSocketDataType(StringRef ui_name, BuildFunc build_fn)
      : SocketDataType(ui_name, std::move(build_fn), SocketTypeCategory::Base)
  {
  }
};

class ListSocketDataType : public SocketDataType {
 public:
  BaseSocketDataType *m_base_type;

  ListSocketDataType(StringRef ui_name, BuildFunc build_fn)
      : SocketDataType(ui_name, std::move(build_fn), SocketTypeCategory::List)
  {
  }
};

class SocketDataTypes {
 public:
};

class SocketDecl {
 public:
  bNodeTree *m_ntree;
  bNode *m_node;

  virtual void build() const = 0;
};

class MockupSocketDecl : public SocketDecl {
 public:
  eNodeSocketInOut m_in_out;
  StringRefNull m_ui_name;
  StringRefNull m_identifier;
  StringRefNull m_idname;

  void build() const override
  {
    nodeAddSocket(
        m_ntree, m_node, m_in_out, m_idname.data(), m_identifier.data(), m_ui_name.data());
  }
};

class NodeDecl {
 private:
  Vector<SocketDecl *> m_inputs;
  Vector<SocketDecl *> m_outputs;
};

static void init_node(bNodeTree *ntree, bNode *node)
{
  MockupSocketDecl decl;
  decl.m_in_out = SOCK_IN;
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
  ntype.poll = [](bNodeType *UNUSED(ntype), bNodeTree *UNUSED(ntree)) { return true; };

  nodeRegisterType(&ntype);
}
