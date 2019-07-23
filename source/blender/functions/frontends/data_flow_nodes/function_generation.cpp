#include "function_generation.hpp"
#include "graph_generation.hpp"

#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"
#include "FN_llvm.hpp"
#include "DNA_node_types.h"

namespace FN {
namespace DataFlowNodes {

static void find_interface_sockets(VirtualNodeTree &vtree,
                                   VTreeDataGraph &data_graph,
                                   DFGraphSocketSetVector &r_inputs,
                                   DFGraphSocketSetVector &r_outputs)
{
  VirtualNode *input_node = vtree.nodes_with_idname("fn_FunctionInputNode").get(0, nullptr);
  VirtualNode *output_node = vtree.nodes_with_idname("fn_FunctionOutputNode").get(0, nullptr);

  if (input_node != nullptr) {
    for (uint i = 0; i < input_node->outputs().size() - 1; i++) {
      VirtualSocket *vsocket = input_node->output(i);
      r_inputs.add_new(data_graph.lookup_socket(vsocket));
    }
  }

  if (output_node != nullptr) {
    for (uint i = 0; i < output_node->inputs().size() - 1; i++) {
      VirtualSocket *vsocket = output_node->input(i);
      r_outputs.add_new(data_graph.lookup_socket(vsocket));
    }
  }
}

static Optional<FunctionGraph> generate_function_graph(VirtualNodeTree &vtree)
{
  Optional<VTreeDataGraph> data_graph_ = generate_graph(vtree);
  if (!data_graph_.has_value()) {
    return {};
  }

  VTreeDataGraph &data_graph = data_graph_.value();

  DFGraphSocketSetVector input_sockets;
  DFGraphSocketSetVector output_sockets;
  find_interface_sockets(vtree, data_graph, input_sockets, output_sockets);

  return FunctionGraph(data_graph.graph(), input_sockets, output_sockets);
}

Optional<SharedFunction> generate_function(bNodeTree *btree)
{
  VirtualNodeTree vtree;
  vtree.add_all_of_tree(btree);
  vtree.freeze_and_index();

  Optional<FunctionGraph> fgraph_ = generate_function_graph(vtree);
  if (!fgraph_.has_value()) {
    return {};
  }

  FunctionGraph fgraph = fgraph_.value();
  // fgraph.graph()->to_dot__clipboard();

  auto fn = fgraph.new_function(btree->id.name);
  fgraph_add_DependenciesBody(fn, fgraph);
  fgraph_add_LLVMBuildIRBody(fn, fgraph);

  fgraph_add_TupleCallBody(fn, fgraph);
  // derive_TupleCallBody_from_LLVMBuildIRBody(fn);
  return fn;
}

}  // namespace DataFlowNodes
}  // namespace FN
