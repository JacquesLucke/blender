#pragma once

#include <functional>

#include "BKE_node_tree.hpp"
#include "FN_data_flow_nodes.hpp"

#include "world_state.hpp"
#include "step_description.hpp"

namespace BParticles {

using BKE::bSocketList;
using BKE::IndexedNodeTree;
using BKE::SocketWithNode;

class ProcessNodeInterface {
 private:
  bNode *m_bnode;
  IndexedNodeTree &m_indexed_tree;
  FN::DataFlowNodes::GeneratedGraph &m_data_graph;
  WorldState &m_world_state;
  ModifierStepDescription &m_step_description;

 public:
  ProcessNodeInterface(bNode *bnode,
                       IndexedNodeTree &indexed_tree,
                       FN::DataFlowNodes::GeneratedGraph &data_graph,
                       WorldState &world_state,
                       ModifierStepDescription &step_description)
      : m_bnode(bnode),
        m_indexed_tree(indexed_tree),
        m_data_graph(data_graph),
        m_world_state(world_state),
        m_step_description(step_description)
  {
  }

  bNode *bnode()
  {
    return m_bnode;
  }

  IndexedNodeTree &indexed_tree()
  {
    return m_indexed_tree;
  }

  FN::DataFlowNodes::GeneratedGraph &data_graph()
  {
    return m_data_graph;
  }

  WorldState &world_state()
  {
    return m_world_state;
  }

  ModifierStepDescription &step_description()
  {
    return m_step_description;
  }

  ArrayRef<SocketWithNode> linked_with_input(uint index)
  {
    bNodeSocket *socket = this->inputs().get(index);
    return m_indexed_tree.linked(socket);
  }

  ArrayRef<SocketWithNode> linked_with_output(uint index)
  {
    bNodeSocket *socket = this->outputs().get(index);
    return m_indexed_tree.linked(socket);
  }

  bSocketList inputs()
  {
    return bSocketList(m_bnode->inputs);
  }

  bSocketList outputs()
  {
    return bSocketList(m_bnode->outputs);
  }

  PointerRNA node_rna()
  {
    return m_indexed_tree.get_rna(m_bnode);
  }
};

using ProcessNodeFunction = std::function<void(ProcessNodeInterface &interface)>;
using ProcessFunctionsMap = SmallMap<std::string, ProcessNodeFunction>;

ProcessFunctionsMap &get_node_processors();

}  // namespace BParticles
