#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

class ExecuteGraph : public TupleCallBody {
 private:
  CompactFunctionGraph m_fgraph;
  /* Just for easy access. */
  CompactDataFlowGraph *m_graph;

 public:
  ExecuteGraph(const CompactFunctionGraph &function_graph)
      : m_fgraph(function_graph), m_graph(function_graph.graph().ptr())
  {
    auto *context = new llvm::LLVMContext();

    for (uint node : m_graph->node_ids()) {
      SharedFunction &fn = m_graph->function_of_node(node);
      if (fn->has_body<TupleCallBody>()) {
        continue;
      }

      if (fn->has_body<LazyInTupleCallBody>()) {
        derive_TupleCallBody_from_LazyInTupleCallBody(fn);
      }

      if (fn->has_body<LLVMBuildIRBody>()) {
        derive_TupleCallBody_from_LLVMBuildIRBody(fn, *context);
      }
    }
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      FunctionSocket socket = m_fgraph.outputs()[i];
      this->compute_socket(fn_in, fn_out, i, socket, ctx);
    }
  }

  void compute_socket(
      Tuple &fn_in, Tuple &out, uint out_index, FunctionSocket socket, ExecutionContext &ctx) const
  {
    if (m_fgraph.inputs().contains(socket)) {
      uint index = m_fgraph.inputs().index(socket);
      Tuple::copy_element(fn_in, index, out, out_index);
    }
    else if (socket.is_input()) {
      this->compute_socket(fn_in, out, out_index, m_graph->origin_of_input(socket), ctx);
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket);
      SharedFunction &fn = m_graph->function_of_node(node_id);
      TupleCallBody *body = fn->body<TupleCallBody>();

      FN_TUPLE_CALL_ALLOC_TUPLES(body, tmp_in, tmp_out);

      uint index = 0;
      for (FunctionSocket input_socket : m_graph->inputs_of_node(node_id)) {
        this->compute_socket(fn_in, tmp_in, index, input_socket, ctx);
        index++;
      }

      SourceInfoStackFrame node_frame(m_graph->source_info_of_node(node_id));
      body->call__setup_stack(tmp_in, tmp_out, ctx, node_frame);

      Tuple::copy_element(tmp_out, m_graph->index_of_output(socket.id()), out, out_index);
    }
  }
};

void fgraph_add_TupleCallBody(SharedFunction &fn, CompactFunctionGraph &fgraph)
{
  fn->add_body(new ExecuteGraph(fgraph));
}

} /* namespace FN */
