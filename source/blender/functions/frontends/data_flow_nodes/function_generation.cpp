#include "function_generation.hpp"
#include "graph_generation.hpp"

#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"
#include "FN_llvm.hpp"
#include "DNA_node_types.h"

namespace FN {
namespace DataFlowNodes {

Optional<SharedFunction> generate_function(bNodeTree *btree)
{
  IndexedNodeTree indexed_btree(btree);

  Optional<FunctionGraph> fgraph_ = generate_function_graph(indexed_btree);
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
