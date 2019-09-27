#include <sstream>

#include "DNA_node_types.h"
#include "RNA_access.h"
#include "FN_types.hpp"

#include "vtree_data_graph_builder.hpp"

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

VTreeDataGraphBuilder::VTreeDataGraphBuilder(VirtualNodeTree &vtree)
    : m_vtree(vtree), m_socket_map(vtree.socket_count(), nullptr), m_type_mappings(MAPPING_types())
{
  this->initialize_type_by_vsocket_map();
}

BLI_NOINLINE void VTreeDataGraphBuilder::initialize_type_by_vsocket_map()
{
  m_type_by_vsocket = Vector<Type *>(m_vtree.socket_count(), nullptr);
  for (VirtualNode *vnode : m_vtree.nodes()) {
    for (VirtualSocket *vsocket : vnode->inputs()) {
      m_type_by_vsocket[vsocket->id()] = m_type_mappings->type_by_idname_or_empty(
          vsocket->idname());
    }
    for (VirtualSocket *vsocket : vnode->outputs()) {
      m_type_by_vsocket[vsocket->id()] = m_type_mappings->type_by_idname_or_empty(
          vsocket->idname());
    }
  }
}

std::unique_ptr<VTreeDataGraph> VTreeDataGraphBuilder::build()
{
  auto data_graph = m_graph_builder.build();

  Array<DataSocket> r_socket_map(m_vtree.socket_count(), DataSocket::None());
  for (uint vsocket_id = 0; vsocket_id < m_vtree.socket_count(); vsocket_id++) {
    BuilderSocket *socket = m_socket_map[vsocket_id];
    if (socket == nullptr) {
      r_socket_map[vsocket_id] = DataSocket::None();
    }
    else if (socket->is_input()) {
      r_socket_map[vsocket_id] = DataSocket::FromInput(((BuilderInputSocket *)socket)->input_id());
    }
    else {
      r_socket_map[vsocket_id] = DataSocket::FromOutput(
          ((BuilderOutputSocket *)socket)->output_id());
    }
  }

  auto *vtree = new VTreeDataGraph(m_vtree, std::move(data_graph), std::move(r_socket_map));
  return std::unique_ptr<VTreeDataGraph>(vtree);
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
    ss << " - Node \"" << m_bnode->name << "\"";
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
    PyObject *ret = PyObject_CallFunction(function, "Os", py_bnode, std::string(msg).c_str());
    Py_DECREF(ret);

    PyGILState_Release(gilstate);
#endif
  }
};

BuilderNode *VTreeDataGraphBuilder::insert_function(Function &fn)
{
  return m_graph_builder.insert_function(fn);
}

BuilderNode *VTreeDataGraphBuilder::insert_matching_function(Function &fn, VirtualNode *vnode)
{
  BuilderNode *node = this->insert_function(fn, vnode);
  this->map_sockets(node, vnode);
  return node;
}

BuilderNode *VTreeDataGraphBuilder::insert_function(Function &fn, VirtualNode *vnode)
{
  BLI_assert(vnode != nullptr);
  NodeSource *source = m_graph_builder.new_source_info<NodeSource>(vnode->btree(), vnode->bnode());
  return m_graph_builder.insert_function(fn, source);
}

BuilderNode *VTreeDataGraphBuilder::insert_placeholder(VirtualNode *vnode)
{
  FunctionBuilder fn_builder;

  Vector<VirtualSocket *> vsocket_inputs;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    if (this->is_data_socket(vsocket)) {
      vsocket_inputs.append(vsocket);
      Type *type = this->query_socket_type(vsocket);
      fn_builder.add_input(vsocket->name(), type);
    }
  }

  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (this->is_data_socket(vsocket)) {
      Type *type = this->query_socket_type(vsocket);
      fn_builder.add_output(vsocket->name(), type);
    }
  }

  std::unique_ptr<Function> fn = fn_builder.build(vnode->name());
  fn->add_body<VNodePlaceholderBody>(vnode, std::move(vsocket_inputs));
  BuilderNode *node = this->insert_function(*fn);
  this->add_resource(std::move(fn), "placeholder function");

  this->map_data_sockets(node, vnode);
  m_placeholder_nodes.append(node);
  return node;
}

ArrayRef<BuilderNode *> VTreeDataGraphBuilder::placeholder_nodes()
{
  return m_placeholder_nodes;
}

void VTreeDataGraphBuilder::insert_link(BuilderOutputSocket *from, BuilderInputSocket *to)
{
  m_graph_builder.insert_link(from, to);
}

void VTreeDataGraphBuilder::insert_links(ArrayRef<BuilderOutputSocket *> from,
                                         ArrayRef<BuilderInputSocket *> to)
{
  BLI_assert(from.size() == to.size());
  for (uint i = 0; i < from.size(); i++) {
    this->insert_link(from[i], to[i]);
  }
}

void VTreeDataGraphBuilder::map_input_socket(BuilderInputSocket *socket, VirtualSocket *vsocket)
{
  BLI_assert(this->is_data_socket(vsocket));
  BLI_assert(vsocket->is_input());
  BLI_assert(socket->is_input());
  BLI_assert(socket->type() == this->query_socket_type(vsocket));
  m_socket_map[vsocket->id()] = socket;
}

void VTreeDataGraphBuilder::map_output_socket(BuilderOutputSocket *socket, VirtualSocket *vsocket)
{
  BLI_assert(this->is_data_socket(vsocket));
  BLI_assert(vsocket->is_output());
  BLI_assert(socket->is_output());
  BLI_assert(socket->type() == this->query_socket_type(vsocket));
  m_socket_map[vsocket->id()] = socket;
}

void VTreeDataGraphBuilder::map_sockets(BuilderNode *node, VirtualNode *vnode)
{
  uint input_amount = node->inputs().size();
  uint output_amount = node->outputs().size();

  BLI_assert(vnode->inputs().size() == input_amount);
  BLI_assert(vnode->outputs().size() == output_amount);

  for (uint i = 0; i < input_amount; i++) {
    this->map_input_socket(node->input(i), vnode->input(i));
  }
  for (uint i = 0; i < output_amount; i++) {
    this->map_output_socket(node->output(i), vnode->output(i));
  }
}

void VTreeDataGraphBuilder::map_data_sockets(BuilderNode *node, VirtualNode *vnode)
{
  uint input_index = 0;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    if (this->is_data_socket(vsocket)) {
      this->map_input_socket(node->input(input_index), vsocket);
      input_index++;
    }
  }

  uint output_index = 0;
  for (VirtualSocket *vsocket : vnode->outputs()) {
    if (this->is_data_socket(vsocket)) {
      this->map_output_socket(node->output(output_index), vsocket);
      output_index++;
    }
  }
}

BuilderInputSocket *VTreeDataGraphBuilder::lookup_input_socket(VirtualSocket *vsocket)
{
  BLI_assert(vsocket->is_input());

  BuilderSocket *socket = m_socket_map[vsocket->id()];
  BLI_assert(socket != nullptr);
  BLI_assert(socket->is_input());
  return (BuilderInputSocket *)socket;
}
BuilderOutputSocket *VTreeDataGraphBuilder::lookup_output_socket(VirtualSocket *vsocket)
{
  BLI_assert(vsocket->is_output());

  BuilderSocket *socket = m_socket_map[vsocket->id()];
  BLI_assert(socket != nullptr);
  BLI_assert(socket->is_output());
  return (BuilderOutputSocket *)socket;
}

bool VTreeDataGraphBuilder::is_input_unlinked(VirtualSocket *vsocket)
{
  BLI_assert(vsocket->is_input());
  if (this->is_data_socket(vsocket)) {
    BuilderInputSocket *socket = this->lookup_input_socket(vsocket);
    return socket->origin() == nullptr;
  }
  else {
    return false;
  }
}

bool VTreeDataGraphBuilder::check_if_sockets_are_mapped(VirtualNode *vnode,
                                                        ArrayRef<VirtualSocket *> vsockets) const
{
  int index = 0;
  for (VirtualSocket *vsocket : vsockets) {
    if (this->is_data_socket(vsocket)) {
      if (m_socket_map[vsocket->id()] == nullptr) {
        std::cout << "Data socket not mapped: " << std::endl;
        std::cout << "    Tree: " << vnode->btree_id()->name << std::endl;
        std::cout << "    Node: " << vnode->name() << std::endl;
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

bool VTreeDataGraphBuilder::verify_data_sockets_mapped(VirtualNode *vnode) const
{
  return (this->check_if_sockets_are_mapped(vnode, vnode->inputs()) &&
          this->check_if_sockets_are_mapped(vnode, vnode->outputs()));
}

VirtualNodeTree &VTreeDataGraphBuilder::vtree() const
{
  return m_vtree;
}

bool VTreeDataGraphBuilder::is_data_socket(VirtualSocket *vsocket) const
{
  return m_type_by_vsocket[vsocket->id()] != nullptr;
}

Type *VTreeDataGraphBuilder::type_by_name(StringRef data_type) const
{
  return m_type_mappings->type_by_name(data_type);
}

Type *VTreeDataGraphBuilder::query_socket_type(VirtualSocket *vsocket) const
{
  BLI_assert(this->is_data_socket(vsocket));
  return m_type_by_vsocket[vsocket->id()];
}

Type *VTreeDataGraphBuilder::query_type_property(VirtualNode *vnode, StringRefNull prop_name) const
{
  PointerRNA rna = vnode->rna();
  return this->type_from_rna(rna, prop_name);
}

Type *VTreeDataGraphBuilder::type_from_rna(PointerRNA &rna, StringRefNull prop_name) const
{
  char type_name[64];
  RNA_string_get(&rna, prop_name.data(), type_name);
  return this->type_by_name(type_name);
}

bool VTreeDataGraphBuilder::has_data_socket(VirtualNode *vnode) const
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

std::string VTreeDataGraphBuilder::to_dot()
{
  return m_graph_builder.to_dot();
}

void VTreeDataGraphBuilder::to_dot__clipboard()
{
  m_graph_builder.to_dot__clipboard();
}

}  // namespace DataFlowNodes
}  // namespace FN
