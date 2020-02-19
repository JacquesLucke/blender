#include <cstring>
#include <typeinfo>

#include "BKE_node.h"
#include "SIM_node_tree.h"

#include "BLI_vector.h"
#include "BLI_string_ref.h"
#include "BLI_set.h"
#include "BLI_linear_allocator.h"
#include "BLI_color.h"
#include "BLI_string.h"
#include "BLI_array_cxx.h"

#include "PIL_time.h"

#include "BKE_context.h"

#include "DNA_space_types.h"

#include "UI_interface.h"

#include "../space_node/node_intern.h"

using BLI::Array;
using BLI::ArrayRef;
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
  uint m_amount;

 public:
  SocketDecl(bNodeTree &ntree, bNode &node, uint amount)
      : m_ntree(ntree), m_node(node), m_amount(amount)
  {
  }

  virtual ~SocketDecl();

  uint amount() const
  {
    return m_amount;
  }

  virtual bool sockets_are_correct(ArrayRef<bNodeSocket *> sockets) const = 0;

  virtual void build() const = 0;
};

SocketDecl::~SocketDecl()
{
}

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
      : SocketDecl(ntree, node, 1),
        m_in_out(in_out),
        m_type(type),
        m_ui_name(ui_name),
        m_identifier(identifier)
  {
  }

  bool sockets_are_correct(ArrayRef<bNodeSocket *> sockets) const override
  {
    if (sockets.size() != 1) {
      return false;
    }

    bNodeSocket *socket = sockets[0];
    if (socket->typeinfo != m_type.m_socket_type) {
      return false;
    }
    if (socket->name != m_ui_name) {
      return false;
    }
    if (socket->identifier != m_identifier) {
      return false;
    }
    return true;
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

  bool sockets_are_correct() const
  {
    if (!this->sockets_are_correct(m_node.inputs, m_inputs)) {
      return false;
    }
    if (!this->sockets_are_correct(m_node.outputs, m_outputs)) {
      return false;
    }
    return true;
  }

 private:
  bool sockets_are_correct(ListBase &sockets_list, ArrayRef<SocketDecl *> decls) const
  {
    Vector<bNodeSocket *, 10> sockets;
    LISTBASE_FOREACH (bNodeSocket *, socket, &sockets_list) {
      sockets.append(socket);
    }

    uint offset = 0;
    for (SocketDecl *decl : decls) {
      uint amount = decl->amount();
      if (offset + amount > sockets.size()) {
        return false;
      }
      ArrayRef<bNodeSocket *> sockets_for_decl = sockets.as_ref().slice(offset, amount);
      if (!decl->sockets_are_correct(sockets_for_decl)) {
        return false;
      }
      offset += amount;
    }
    if (offset != sockets.size()) {
      return false;
    }
    return true;
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

  template<typename T> T *node_storage()
  {
#ifdef DEBUG
    const char *type_name = typeid(T).name();
    const char *expected_name = m_node_decl.m_node.typeinfo->storagename;
    BLI_assert(strstr(type_name, expected_name));
#endif
    return (T *)m_node_decl.m_node.storage;
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

using DeclareNodeFunc = void (*)(NodeBuilder &builder);

static void declare_test_node(NodeBuilder &builder)
{
  MyTestNodeStorage *storage = builder.node_storage<MyTestNodeStorage>();

  builder.fixed_input("id1", "ID 1", *data_socket_float);
  builder.fixed_input("id2", "ID 2", *data_socket_int);
  builder.fixed_input("id4", "ID 4", *data_socket_int_list);
  builder.fixed_output("id3", "ID 3", *data_socket_float);

  for (int i = 0; i < storage->x; i++) {
    builder.fixed_input(
        "id" + std::to_string(i), "Hello " + std::to_string(i), *data_socket_float_list);
  }
}

static void init_node(bNodeTree *ntree, bNode *node)
{
  LinearAllocator<> allocator;
  NodeDecl node_decl{*ntree, *node};
  NodeBuilder node_builder{allocator, node_decl};
  /* TODO: free storage */
  node->storage = MEM_callocN(sizeof(MyTestNodeStorage), __func__);
  declare_test_node(node_builder);
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
  strcpy(ntype.storagename, "MyTestNodeStorage");
  ntype.type = NODE_CUSTOM;

  ntype.initfunc = init_node;
  ntype.poll = [](bNodeType *UNUSED(ntype), bNodeTree *UNUSED(ntree)) { return true; };
  ntype.userdata = (void *)declare_test_node;

  ntype.draw_nodetype = node_draw_default;
  ntype.draw_nodetype_prepare = node_update_default;
  ntype.select_area_func = node_select_area_default;
  ntype.tweak_area_func = node_tweak_area_default;
  ntype.draw_buttons_ex = nullptr;
  ntype.resize_area_func = node_resize_area_default;

  ntype.draw_buttons = [](uiLayout *layout, struct bContext *UNUSED(C), struct PointerRNA *ptr) {
    bNode *node = (bNode *)ptr->data;
    MyTestNodeStorage *storage = (MyTestNodeStorage *)node->storage;
    uiBut *but = uiDefButI(uiLayoutGetBlock(layout),
                           UI_BTYPE_NUM,
                           0,
                           "X value",
                           0,
                           0,
                           50,
                           50,
                           &storage->x,
                           -1000,
                           1000,
                           3,
                           20,
                           "my x value");
    uiItemL(layout, "Hello World", 0);
    UI_but_func_set(
        but,
        [](bContext *C, void *UNUSED(arg1), void *UNUSED(arg2)) {
          bNodeTree *ntree = CTX_wm_space_node(C)->edittree;
          ntree->update = NTREE_UPDATE;
          ntreeUpdateTree(CTX_data_main(C), ntree);
        },
        nullptr,
        nullptr);
  };

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

/* TODO: actually call this function */
void free_socket_data_types()
{
  delete socket_data_types;
  delete data_socket_float;
  delete data_socket_int;
  delete data_socket_float_list;
  delete data_socket_int_list;
}

void update_sim_node_tree(bNodeTree *ntree)
{
  Vector<bNode *> nodes;
  for (bNode *node : BLI::IntrusiveListBaseWrapper<bNode>(ntree->nodes)) {
    nodes.append(node);
  }

  LinearAllocator<> allocator;

  for (bNode *node : nodes) {
    NodeDecl node_decl{*ntree, *node};
    NodeBuilder builder{allocator, node_decl};
    DeclareNodeFunc fn = (DeclareNodeFunc)node->typeinfo->userdata;
    fn(builder);

    if (!node_decl.sockets_are_correct()) {
      std::cout << "Rebuild\n";
      nodeRemoveAllSockets(ntree, node);
      node_decl.build();
    }
    else {
      std::cout << "Don't rebuild\n";
    }
  }
}
