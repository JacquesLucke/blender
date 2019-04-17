#include "builder.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"
#include "util_wrappers.hpp"

#include "RNA_access.h"

#include <sstream>

#ifdef WITH_PYTHON
#  include <Python.h>

extern "C" {
PyObject *pyrna_struct_CreatePyObject(PointerRNA *rna);
}
#endif

#ifdef WITH_PYTHON
static PyObject *get_py_bnode(bNodeTree *btree, bNode *bnode)
{
  PointerRNA rna;
  RNA_pointer_create(&btree->id, &RNA_Node, bnode, &rna);
  return pyrna_struct_CreatePyObject(&rna);
}
#endif

namespace FN {
namespace DataFlowNodes {

class NodeSource : public SourceInfo {
 private:
  bNodeTree *m_btree;
  bNode *m_bnode;

 public:
  NodeSource(bNodeTree *btree, bNode *bnode) : m_btree(btree), m_bnode(bnode)
  {
  }

  std::string to_string() const override
  {
    std::stringstream ss;
    ss << "NodeTree \"" << m_btree->id.name + 2 << "\"";
    ss << " - Node \"" << m_bnode->name << "\"";
    return ss.str();
  }

  void handle_warning(std::string msg) const override
  {
#ifdef WITH_PYTHON
    PyGILState_STATE gilstate;
    gilstate = PyGILState_Ensure();

    PyObject *module = PyImport_ImportModule("function_nodes.problems");
    PyObject *globals = PyModule_GetDict(module);
    PyObject *function = PyDict_GetItemString(globals, "report_warning");

    PyObject *py_bnode = get_py_bnode(m_btree, m_bnode);
    PyObject *ret = PyObject_CallFunction(function, "Os", py_bnode, msg.c_str());
    Py_DECREF(ret);

    PyGILState_Release(gilstate);
#endif
  }
};

class LinkSource : public SourceInfo {
 private:
  bNodeTree *m_btree;
  bNodeLink *m_blink;

 public:
  LinkSource(bNodeTree *btree, bNodeLink *blink) : m_btree(btree), m_blink(blink)
  {
  }

  std::string to_string() const override
  {
    std::stringstream ss;
    ss << "NodeTree \"" << m_btree->id.name + 2 << "\"";
    ss << " - Link";
    return ss.str();
  }
};

Node *GraphBuilder::insert_function(SharedFunction &fn)
{
  return m_graph->insert(fn);
}

Node *GraphBuilder::insert_matching_function(SharedFunction &fn, bNode *bnode)
{
  Node *node = this->insert_function(fn, bnode);
  this->map_sockets(node, bnode);
  return node;
}

Node *GraphBuilder::insert_function(SharedFunction &fn, bNode *bnode)
{
  BLI_assert(bnode != nullptr);
  NodeSource *source = m_graph->new_source_info<NodeSource>(m_btree, bnode);
  return m_graph->insert(fn, source);
}

Node *GraphBuilder::insert_function(SharedFunction &fn, bNodeLink *blink)
{
  BLI_assert(blink != nullptr);
  LinkSource *source = m_graph->new_source_info<LinkSource>(m_btree, blink);
  return m_graph->insert(fn, source);
}

void GraphBuilder::insert_link(Socket a, Socket b)
{
  m_graph->link(a, b);
}

void GraphBuilder::map_socket(Socket socket, bNodeSocket *bsocket)
{
  BLI_assert(this->is_data_socket(bsocket) ? socket.type() == this->query_socket_type(bsocket) :
                                             true);
  m_socket_map.add(bsocket, socket);
}

void GraphBuilder::map_sockets(Node *node, struct bNode *bnode)
{
  BLI_assert(BLI_listbase_count(&bnode->inputs) == node->input_amount());
  BLI_assert(BLI_listbase_count(&bnode->outputs) == node->output_amount());

  uint input_index = 0;
  for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
    this->map_socket(node->input(input_index), bsocket);
    input_index++;
  }

  uint output_index = 0;
  for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
    this->map_socket(node->output(output_index), bsocket);
    output_index++;
  }
}

void GraphBuilder::map_data_sockets(Node *node, struct bNode *bnode)
{
  uint input_index = 0;
  for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
    if (this->is_data_socket(bsocket)) {
      this->map_socket(node->input(input_index), bsocket);
      input_index++;
    }
  }

  uint output_index = 0;
  for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
    if (this->is_data_socket(bsocket)) {
      this->map_socket(node->output(output_index), bsocket);
      output_index++;
    }
  }
}

void GraphBuilder::map_input(Socket socket, struct bNode *bnode, uint index)
{
  BLI_assert(socket.is_input());
  auto bsocket = (bNodeSocket *)BLI_findlink(&bnode->inputs, index);
  this->map_socket(socket, bsocket);
}

void GraphBuilder::map_output(Socket socket, struct bNode *bnode, uint index)
{
  BLI_assert(socket.is_output());
  auto bsocket = (bNodeSocket *)BLI_findlink(&bnode->outputs, index);
  this->map_socket(socket, bsocket);
}

Socket GraphBuilder::lookup_socket(struct bNodeSocket *bsocket)
{
  BLI_assert(m_socket_map.contains(bsocket));
  return m_socket_map.lookup(bsocket);
}

bool GraphBuilder::check_if_sockets_are_mapped(struct bNode *bnode, bSocketList bsockets) const
{
  int index = 0;
  for (bNodeSocket *bsocket : bsockets) {
    if (this->is_data_socket(bsocket)) {
      if (!m_socket_map.contains(bsocket)) {
        std::cout << "Data Socket not mapped: " << std::endl;
        std::cout << "    Tree: " << m_btree->id.name << std::endl;
        std::cout << "    Node: " << bnode->name << std::endl;
        if (bsocket->in_out == SOCK_IN) {
          std::cout << "    Input";
        }
        else {
          std::cout << "    Output";
        }
        std::cout << " Index: " << index << std::endl;
        return false;
      }
    }
    index++;
  }
  return true;
}

bool GraphBuilder::verify_data_sockets_mapped(struct bNode *bnode) const
{
  return (this->check_if_sockets_are_mapped(bnode, bSocketList(&bnode->inputs)) &&
          this->check_if_sockets_are_mapped(bnode, bSocketList(&bnode->outputs)));
}

struct bNodeTree *GraphBuilder::btree() const
{
  return m_btree;
}

struct ID *GraphBuilder::btree_id() const
{
  return &m_btree->id;
}

bool GraphBuilder::is_data_socket(bNodeSocket *bsocket) const
{
  PointerRNA rna = this->get_rna(bsocket);
  return RNA_struct_find_property(&rna, "data_type") != NULL;
}

SharedType &GraphBuilder::type_by_name(const char *data_type) const
{
  if (STREQ(data_type, "Float")) {
    return Types::GET_TYPE_float();
  }
  else if (STREQ(data_type, "Integer")) {
    return Types::GET_TYPE_int32();
  }
  else if (STREQ(data_type, "Vector")) {
    return Types::GET_TYPE_fvec3();
  }
  else if (STREQ(data_type, "Boolean")) {
    return Types::GET_TYPE_bool();
  }
  else if (STREQ(data_type, "Float List")) {
    return Types::GET_TYPE_float_list();
  }
  else if (STREQ(data_type, "Vector List")) {
    return Types::GET_TYPE_fvec3_list();
  }
  else if (STREQ(data_type, "Integer List")) {
    return Types::GET_TYPE_int32_list();
  }
  else {
    BLI_assert(false);
    return *(SharedType *)nullptr;
  }
}

SharedType &GraphBuilder::query_socket_type(bNodeSocket *bsocket) const
{
  std::string data_type = this->query_socket_type_name(bsocket);
  return this->type_by_name(data_type.c_str());
}

std::string GraphBuilder::query_socket_name(bNodeSocket *bsocket) const
{
  return bsocket->name;
}

PointerRNA GraphBuilder::get_rna(bNode *bnode) const
{
  PointerRNA rna;
  RNA_pointer_create(this->btree_id(), &RNA_Node, bnode, &rna);
  return rna;
}

PointerRNA GraphBuilder::get_rna(bNodeSocket *bsocket) const
{
  PointerRNA rna;
  RNA_pointer_create(this->btree_id(), &RNA_NodeSocket, bsocket, &rna);
  return rna;
}

SharedType &GraphBuilder::query_type_property(bNode *bnode, const char *prop_name) const
{
  PointerRNA rna = this->get_rna(bnode);
  return this->type_from_rna(rna, prop_name);
}

SharedType &GraphBuilder::type_from_rna(PointerRNA &rna, const char *prop_name) const
{
  char type_name[64];
  RNA_string_get(&rna, prop_name, type_name);
  return this->type_by_name(type_name);
}

std::string GraphBuilder::query_socket_type_name(bNodeSocket *bsocket) const
{
  BLI_assert(this->is_data_socket(bsocket));
  PointerRNA rna = this->get_rna(bsocket);
  char type_name[64];
  RNA_string_get(&rna, "data_type", type_name);
  return type_name;
}

}  // namespace DataFlowNodes
}  // namespace FN
