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

static ValueOrError<FunctionGraph> generate_function_graph(VirtualNodeTree &vtree)
{
  ValueOrError<VTreeDataGraph> data_graph_or_error = generate_graph(vtree);
  if (data_graph_or_error.is_error()) {
    return data_graph_or_error.error();
  }

  VTreeDataGraph data_graph = data_graph_or_error.extract_value();

  DFGraphSocketSetVector input_sockets;
  DFGraphSocketSetVector output_sockets;
  find_interface_sockets(vtree, data_graph, input_sockets, output_sockets);

  return FunctionGraph(data_graph.graph(), input_sockets, output_sockets);
}

ValueOrError<SharedFunction> generate_function(bNodeTree *btree)
{
  VirtualNodeTree vtree;
  vtree.add_all_of_tree(btree);
  vtree.freeze_and_index();

  ValueOrError<FunctionGraph> fgraph_or_error = generate_function_graph(vtree);
  if (fgraph_or_error.is_error()) {
    return fgraph_or_error.error();
  }

  FunctionGraph fgraph = fgraph_or_error.extract_value();
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
