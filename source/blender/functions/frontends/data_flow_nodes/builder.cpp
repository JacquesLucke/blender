#include "builder.hpp"
#include "type_mappings.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

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

BTreeGraphBuilder::BTreeGraphBuilder(VirtualNodeTree &vtree,
                                     DataFlowGraphBuilder &graph,
                                     Map<VirtualSocket *, DFGB_Socket> &socket_map)
    : m_graph(graph),
      m_vtree(vtree),
      m_socket_map(socket_map),
      m_type_by_idname(get_type_by_idname_map()),
      m_type_by_data_type(get_type_by_data_type_map())
{
}

class NodeSource : public SourceInfo {
 private:
  bNodeTree *m_btree;
  bNode *m_bnode;

 public:
  NodeSource(bNodeTree *btree, bNode *vnode) : m_btree(btree), m_bnode(vnode)
  {
  }

  std::string to_string() const override
  {
    std::stringstream ss;
    ss << "NodeTree \"" << m_btree->id.name + 2 << "\"";
    ss << " - DFGB_Node \"" << m_bnode->name << "\"";
    return ss.str();
  }

  void handle_warning(StringRef msg) const override
  {
#ifdef WITH_PYTHON
    PyGILState_STATE gilstate;
    gilstate = PyGILState_Ensure();

    PyObject *module = PyImport_ImportModule("nodes.problems");
    PyObject *globals = PyModule_GetDict(module);
    PyObject *function = PyDict_GetItemString(globals, "report_warning");

    PyObject *py_bnode = get_py_bnode(m_btree, m_bnode);
    PyObject *ret = PyObject_CallFunction(function, "Os", py_bnode, msg.to_std_string().c_str());
    Py_DECREF(ret);

    PyGILState_Release(gilstate);
#endif
  }
};

DFGB_Node *BTreeGraphBuilder::insert_function(SharedFunction &fn)
{
  return m_graph.insert_function(fn);
}

DFGB_Node *BTreeGraphBuilder::insert_matching_function(SharedFunction &fn, VirtualNode *vnode)
{
  DFGB_Node *node = this->insert_function(fn, vnode);
  this->map_sockets(node, vnode);
  return node;
}

DFGB_Node *BTreeGraphBuilder::insert_function(SharedFunction &fn, VirtualNode *vnode)
{
  BLI_assert(vnode != nullptr);
  NodeSource *source = m_graph.new_source_info<NodeSource>(vnode->btree(), vnode->bnode());
  return m_graph.insert_function(fn, source);
}

void BTreeGraphBuilder::insert_link(DFGB_Socket a, DFGB_Socket b)
{
  m_graph.insert_link(a, b);
}

void BTreeGraphBuilder::insert_links(ArrayRef<DFGB_Socket> a, ArrayRef<DFGB_Socket> b)
{
  BLI_assert(a.size() == b.size());
  for (uint i = 0; i < a.size(); i++) {
    this->insert_link(a[i], b[i]);
  }
}

void BTreeGraphBuilder::map_socket(DFGB_Socket socket, VirtualSocket *vsocket)
{
  BLI_assert(this->is_data_socket(vsocket) ? socket.type() == this->query_socket_type(vsocket) :
                                             true);
  m_socket_map.add(vsocket, socket);
}

void BTreeGraphBuilder::map_sockets(DFGB_Node *node, VirtualNode *vnode)
{
  BLI_assert(vnode->inputs().size() == node->input_amount());
  BLI_assert(vnode->outputs().size() == node->output_amount());

  uint input_index = 0;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    this->map_socket(node->input(input_index), vsocket);
    input_index++;
  }

  uint output_index = 0;
  for (VirtualSocket *vsocket : vnode->outputs()) {
    this->map_socket(node->output(output_index), vsocket);
    output_index++;
  }
}

void BTreeGraphBuilder::map_data_sockets(DFGB_Node *node, VirtualNode *vnode)
{
  uint input_index = 0;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    if (this->is_data_socket(vsocket)) {
      this->map_socket(node->input(input_index), vsocket);
      input_index++;
    }
  }

  uint output_index = 0;
  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (this->is_data_socket(vsocket)) {
      this->map_socket(node->output(output_index), vsocket);
      output_index++;
    }
  }
}

void BTreeGraphBuilder::map_input(DFGB_Socket socket, VirtualNode *vnode, uint index)
{
  BLI_assert(socket.is_input());
  VirtualSocket *vsocket = vnode->input(index);
  this->map_socket(socket, vsocket);
}

void BTreeGraphBuilder::map_output(DFGB_Socket socket, VirtualNode *vnode, uint index)
{
  BLI_assert(socket.is_output());
  VirtualSocket *vsocket = vnode->output(index);
  this->map_socket(socket, vsocket);
}

DFGB_Socket BTreeGraphBuilder::lookup_socket(VirtualSocket *vsocket)
{
  BLI_assert(m_socket_map.contains(vsocket));
  return m_socket_map.lookup(vsocket);
}

bool BTreeGraphBuilder::check_if_sockets_are_mapped(VirtualNode *vnode,
                                                    ArrayRef<VirtualSocket *> vsockets) const
{
  int index = 0;
  for (VirtualSocket *vsocket : vsockets) {
    if (this->is_data_socket(vsocket)) {
      if (!m_socket_map.contains(vsocket)) {
        std::cout << "Data DFGB_Socket not mapped: " << std::endl;
        std::cout << "    Tree: " << vnode->btree_id()->name << std::endl;
        std::cout << "    DFGB_Node: " << vnode->name() << std::endl;
        if (vsocket->is_input()) {
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

bool BTreeGraphBuilder::verify_data_sockets_mapped(VirtualNode *vnode) const
{
  return (this->check_if_sockets_are_mapped(vnode, vnode->inputs()) &&
          this->check_if_sockets_are_mapped(vnode, vnode->outputs()));
}

VirtualNodeTree &BTreeGraphBuilder::vtree() const
{
  return m_vtree;
}

bool BTreeGraphBuilder::is_data_socket(VirtualSocket *vsocket) const
{
  return m_type_by_idname.contains(vsocket->bsocket()->idname);
}

SharedType &BTreeGraphBuilder::type_by_name(StringRef data_type) const
{
  return m_type_by_data_type.lookup_ref(data_type);
}

SharedType &BTreeGraphBuilder::query_socket_type(VirtualSocket *vsocket) const
{
  return m_type_by_idname.lookup_ref(vsocket->bsocket()->idname);
}

std::string BTreeGraphBuilder::query_socket_name(VirtualSocket *vsocket) const
{
  return vsocket->bsocket()->name;
}

SharedType &BTreeGraphBuilder::query_type_property(VirtualNode *vnode,
                                                   StringRefNull prop_name) const
{
  PointerRNA rna = vnode->rna();
  return this->type_from_rna(rna, prop_name);
}

SharedType &BTreeGraphBuilder::type_from_rna(PointerRNA &rna, StringRefNull prop_name) const
{
  char type_name[64];
  RNA_string_get(&rna, prop_name, type_name);
  return this->type_by_name(type_name);
}

bool BTreeGraphBuilder::has_data_socket(VirtualNode *vnode) const
{
  for (VirtualSocket *vsocket : vnode->inputs()) {
    if (this->is_data_socket(vsocket)) {
      return true;
    }
  }
  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (this->is_data_socket(vsocket)) {
      return true;
    }
  }
  return false;
}

}  // namespace DataFlowNodes
}  // namespace FN
