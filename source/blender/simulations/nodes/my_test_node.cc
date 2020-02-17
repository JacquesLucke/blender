#include <cstring>

#include "BKE_node.h"
#include "SIM_node_tree.h"

#include "BLI_vector.h"
#include "BLI_string_ref.h"
#include "BLI_set.h"
#include "BLI_linear_allocator.h"
#include "BLI_color.h"
#include "BLI_string.h"

#include "UI_interface.h"

using BLI::LinearAllocator;
using BLI::rgba_f;
using BLI::Set;
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
  std::string m_ui_name;
  bNodeSocketType *m_socket_type;
  SocketTypeCategory m_category;

  SocketDataType(StringRef ui_name, bNodeSocketType *socket_type, SocketTypeCategory category)
      : m_ui_name(ui_name), m_socket_type(socket_type), m_category(category)
  {
  }

  bNodeSocket *build(bNodeTree &ntree,
                     bNode &node,
                     eNodeSocketInOut in_out,
                     StringRef identifier,
                     StringRef ui_name) const
  {
    return nodeAddSocket(
        &ntree, &node, in_out, m_socket_type->idname, identifier.data(), ui_name.data());
  }
};

class BaseSocketDataType : public SocketDataType {
 public:
  ListSocketDataType *m_list_type;

  BaseSocketDataType(StringRef ui_name, bNodeSocketType *socket_type)
      : SocketDataType(ui_name, socket_type, SocketTypeCategory::Base)
  {
  }
};

class ListSocketDataType : public SocketDataType {
 public:
  BaseSocketDataType *m_base_type;

  ListSocketDataType(StringRef ui_name, bNodeSocketType *socket_type)
      : SocketDataType(ui_name, socket_type, SocketTypeCategory::List)
  {
  }
};

class DataTypesInfo {
 private:
  Set<SocketDataType *> m_data_types;

 public:
  void add_data_type(SocketDataType *data_type)
  {
    m_data_types.add_new(data_type);
  }
};

static DataTypesInfo *socket_data_types;

static BaseSocketDataType *data_socket_float;
static BaseSocketDataType *data_socket_int;
static ListSocketDataType *data_socket_float_list;
static ListSocketDataType *data_socket_int_list;

class SocketDecl {
 protected:
  bNodeTree &m_ntree;
  bNode &m_node;

 public:
  SocketDecl(bNodeTree &ntree, bNode &node) : m_ntree(ntree), m_node(node)
  {
  }

  virtual void build() const = 0;
};

class FixedTypeSocketDecl : public SocketDecl {
  eNodeSocketInOut m_in_out;
  SocketDataType &m_type;
  StringRefNull m_ui_name;
  StringRefNull m_identifier;

 public:
  FixedTypeSocketDecl(bNodeTree &ntree,
                      bNode &node,
                      eNodeSocketInOut in_out,
                      SocketDataType &type,
                      StringRefNull ui_name,
                      StringRefNull identifier)
      : SocketDecl(ntree, node),
        m_in_out(in_out),
        m_type(type),
        m_ui_name(ui_name),
        m_identifier(identifier)
  {
  }

  void build() const override
  {
    m_type.build(m_ntree, m_node, m_in_out, m_identifier, m_ui_name);
  }
};

class NodeDecl {
 public:
  bNodeTree &m_ntree;
  bNode &m_node;
  Vector<SocketDecl *> m_inputs;
  Vector<SocketDecl *> m_outputs;

  NodeDecl(bNodeTree &ntree, bNode &node) : m_ntree(ntree), m_node(node)
  {
  }

  void build() const
  {
    for (SocketDecl *decl : m_inputs) {
      decl->build();
    }
    for (SocketDecl *decl : m_outputs) {
      decl->build();
    }
  }
};

class NodeBuilder {
 private:
  LinearAllocator<> &m_allocator;
  NodeDecl &m_node_decl;

 public:
  NodeBuilder(LinearAllocator<> &allocator, NodeDecl &node_decl)
      : m_allocator(allocator), m_node_decl(node_decl)
  {
  }

  void fixed_input(StringRef identifier, StringRef ui_name, SocketDataType &type)
  {
    FixedTypeSocketDecl *decl = m_allocator.construct<FixedTypeSocketDecl>(
        m_node_decl.m_ntree,
        m_node_decl.m_node,
        SOCK_IN,
        type,
        m_allocator.copy_string(ui_name),
        m_allocator.copy_string(identifier));
    m_node_decl.m_inputs.append(decl);
  }

  void fixed_output(StringRef identifier, StringRef ui_name, SocketDataType &type)
  {
    FixedTypeSocketDecl *decl = m_allocator.construct<FixedTypeSocketDecl>(
        m_node_decl.m_ntree,
        m_node_decl.m_node,
        SOCK_OUT,
        type,
        m_allocator.copy_string(ui_name),
        m_allocator.copy_string(identifier));
    m_node_decl.m_outputs.append(decl);
  }
};

static void init_node(bNodeTree *ntree, bNode *node)
{
  LinearAllocator<> allocator;
  NodeDecl node_decl{*ntree, *node};
  NodeBuilder node_builder{allocator, node_decl};

  node_builder.fixed_input("id1", "ID 1", *data_socket_float);
  node_builder.fixed_input("id2", "ID 2", *data_socket_int);
  node_builder.fixed_input("id4", "ID 4", *data_socket_int_list);
  node_builder.fixed_output("id3", "ID 3", *data_socket_float);
  node_builder.fixed_output("id5", "ID 5", *data_socket_float_list);

  node_decl.build();
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

static bNodeSocketType *register_new_simple_socket_type(StringRefNull idname, rgba_f color)
{
  bNodeSocketType *stype = (bNodeSocketType *)MEM_callocN(sizeof(bNodeSocketType), __func__);
  BLI_strncpy(stype->idname, idname.data(), sizeof(stype->idname));
  stype->draw = [](struct bContext *UNUSED(C),
                   struct uiLayout *layout,
                   struct PointerRNA *UNUSED(ptr),
                   struct PointerRNA *UNUSED(node_ptr),
                   const char *text) { uiItemL(layout, text, 0); };

  stype->userdata = new rgba_f(color);
  stype->free_userdata = [](void *userdata) { delete (rgba_f *)userdata; };

  stype->draw_color = [](struct bContext *UNUSED(C),
                         struct PointerRNA *UNUSED(ptr),
                         struct PointerRNA *UNUSED(node_ptr),
                         const void *userdata,
                         float *r_color) {
    rgba_f color = *(rgba_f *)userdata;
    *(rgba_f *)r_color = color;
  };
  nodeRegisterSocketType(stype);
  return stype;
}

void init_socket_data_types()
{
  register_new_simple_socket_type("NodeSocketFloatList", {0.63, 0.63, 0.63, 0.5});
  register_new_simple_socket_type("NodeSocketIntList", {0.06, 0.52, 0.15, 0.5});

  data_socket_float = new BaseSocketDataType("Float", nodeSocketTypeFind("NodeSocketFloat"));
  data_socket_int = new BaseSocketDataType("Integer", nodeSocketTypeFind("NodeSocketInt"));
  data_socket_float_list = new ListSocketDataType("Float List",
                                                  nodeSocketTypeFind("NodeSocketFloatList"));
  data_socket_int_list = new ListSocketDataType("Integer List",
                                                nodeSocketTypeFind("NodeSocketIntList"));

  data_socket_float->m_list_type = data_socket_float_list;
  data_socket_float_list->m_base_type = data_socket_float;
  data_socket_int->m_list_type = data_socket_int_list;
  data_socket_int_list->m_base_type = data_socket_int;

  socket_data_types = new DataTypesInfo();
  socket_data_types->add_data_type(data_socket_float);
  socket_data_types->add_data_type(data_socket_int);
  socket_data_types->add_data_type(data_socket_float_list);
  socket_data_types->add_data_type(data_socket_int_list);
}

void free_socket_data_types()
{
  delete socket_data_types;
  delete data_socket_float;
  delete data_socket_int;
  delete data_socket_float_list;
  delete data_socket_int_list;
}
