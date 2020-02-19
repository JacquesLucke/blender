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

using DeclareNodeFunc = std::function<void(NodeBuilder &node_builder)>;
using InitStorageFunc = std::function<void *()>;
using FreeStorageFunc = std::function<void(void *)>;
using DrawFunc =
    std::function<void(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr)>;
template<typename T> using TypedInitStorageFunc = std::function<void(T *)>;

struct NodeTypeCallbacks {
  DeclareNodeFunc m_declare_node;
  InitStorageFunc m_init_storage;
  FreeStorageFunc m_free_storage;
  DrawFunc m_draw;
};

static void init_node(bNodeTree *ntree, bNode *node)
{
  NodeTypeCallbacks &callbacks = *(NodeTypeCallbacks *)node->typeinfo->userdata;

  LinearAllocator<> allocator;
  NodeDecl node_decl{*ntree, *node};
  NodeBuilder node_builder{allocator, node_decl};
  node->storage = callbacks.m_init_storage();
  callbacks.m_declare_node(node_builder);
  node_decl.build();
}

static void setup_node_base(bNodeType *ntype,
                            StringRef idname,
                            StringRef ui_name,
                            StringRef ui_description,
                            DeclareNodeFunc declare_fn)
{
  memset(ntype, 0, sizeof(bNodeType));
  ntype->minwidth = 20;
  ntype->minheight = 20;
  ntype->maxwidth = 1000;
  ntype->maxheight = 1000;
  ntype->height = 100;
  ntype->width = 140;
  ntype->type = NODE_CUSTOM;

  idname.copy(ntype->idname);
  ui_name.copy(ntype->ui_name);
  ui_description.copy(ntype->ui_description);

  NodeTypeCallbacks *callbacks = new NodeTypeCallbacks();
  ntype->userdata = (void *)callbacks;
  ntype->free_userdata = [](void *userdata) { delete (NodeTypeCallbacks *)userdata; };

  callbacks->m_declare_node = declare_fn;
  callbacks->m_init_storage = []() { return nullptr; };

  ntype->poll = [](bNodeType *UNUSED(ntype), bNodeTree *UNUSED(ntree)) { return true; };
  ntype->initfunc = init_node;

  ntype->draw_buttons = [](struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr) {
    bNode *node = (bNode *)ptr->data;
    NodeTypeCallbacks *callbacks = (NodeTypeCallbacks *)node->typeinfo->userdata;
    callbacks->m_draw(layout, C, ptr);
  };

  ntype->draw_nodetype = node_draw_default;
  ntype->draw_nodetype_prepare = node_update_default;
  ntype->select_area_func = node_select_area_default;
  ntype->tweak_area_func = node_tweak_area_default;
  ntype->resize_area_func = node_resize_area_default;
  ntype->draw_buttons_ex = nullptr;
}

static void setup_node_storage(bNodeType *ntype,
                               StringRef storage_name,
                               InitStorageFunc init_storage_fn,
                               FreeStorageFunc free_storage_fn)
{
  storage_name.copy(ntype->storagename);
  NodeTypeCallbacks *callbacks = (NodeTypeCallbacks *)ntype->userdata;
  callbacks->m_init_storage = init_storage_fn;
  callbacks->m_free_storage = free_storage_fn;
}

template<typename T>
static void setup_node_storage(bNodeType *ntype,
                               StringRef storage_name,
                               TypedInitStorageFunc<T> init_storage_fn)
{
  setup_node_storage(
      ntype,
      storage_name,
      [init_storage_fn]() {
        void *buffer = MEM_callocN(sizeof(T), __func__);
        init_storage_fn((T *)buffer);
        return buffer;
      },
      [](void *buffer) { MEM_freeN(buffer); });
}

static void setup_node_draw(bNodeType *ntype, DrawFunc draw_fn)
{
  NodeTypeCallbacks *callbacks = (NodeTypeCallbacks *)ntype->userdata;
  callbacks->m_draw = draw_fn;
}

void register_node_type_my_test_node()
{
  {
    static bNodeType ntype;
    setup_node_base(&ntype, "MyTestNode", "My Test Node", "My Description", declare_test_node);
    setup_node_storage<MyTestNodeStorage>(
        &ntype, "MyTestNodeStorage", [](MyTestNodeStorage *storage) { storage->x = 3; });
    setup_node_draw(&ntype,
                    [](uiLayout *layout, struct bContext *UNUSED(C), struct PointerRNA *ptr) {
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
                    });

    nodeRegisterType(&ntype);
  }
  {
    static bNodeType ntype;
    setup_node_base(&ntype, "MyTestNode2", "Node 2", "Description", [](NodeBuilder &node_builder) {
      node_builder.fixed_input("a", "A", *data_socket_float);
      node_builder.fixed_input("b", "B", *data_socket_float);
      node_builder.fixed_output("result", "Result", *data_socket_float);
    });
    nodeRegisterType(&ntype);
  }
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
    NodeTypeCallbacks *callbacks = (NodeTypeCallbacks *)node->typeinfo->userdata;
    callbacks->m_declare_node(builder);

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
