#include "fgraph_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN {

	class ExecuteGraph : public TupleCallBody {
	private:
		SharedDataFlowGraph m_graph;
		SmallSocketSetVector m_inputs;
		SmallSocketSetVector m_outputs;

	public:
		ExecuteGraph(const FunctionGraph &function_graph)
			: m_graph(function_graph.graph()),
			  m_inputs(function_graph.inputs()),
			  m_outputs(function_graph.outputs())
		{
			auto *context = new llvm::LLVMContext();

			for (Node *node : m_graph->all_nodes()) {
				if (node->function()->has_body<TupleCallBody>()) {
					continue;
				}

				if (node->function()->has_body<LLVMBuildIRBody>()) {
					derive_TupleCallBody_from_LLVMBuildIRBody(node->function(), *context);
				}
			}
		}

		void call(Tuple &fn_in, Tuple &fn_out) const override
		{
			for (uint i = 0; i < m_outputs.size(); i++) {
				this->compute_socket(fn_in, fn_out, i, m_outputs[i]);
			}
		}

		void compute_socket(Tuple &fn_in, Tuple &out, uint out_index, Socket socket) const
		{
			if (m_inputs.contains(socket)) {
				uint index = m_inputs.index(socket);
				Tuple::copy_element(fn_in, index, out, out_index);
			}
			else if (socket.is_input()) {
				this->compute_socket(fn_in, out, out_index, socket.origin());
			}
			else {
				Node *node = socket.node();
				TupleCallBody *body = node->function()->body<TupleCallBody>();

				FN_TUPLE_STACK_ALLOC(tmp_in, body->meta_in());
				FN_TUPLE_STACK_ALLOC(tmp_out, body->meta_out());

				for (uint i = 0; i < node->input_amount(); i++) {
					this->compute_socket(fn_in, tmp_in, i, node->input(i));
				}

				body->call(tmp_in, tmp_out);

				Tuple::copy_element(tmp_out, socket.index(), out, out_index);
			}
		}
	};

	void fgraph_add_TupleCallBody(
		SharedFunction &fn,
		FunctionGraph &fgraph)
	{
		fn->add_body(new ExecuteGraph(fgraph));
	}

} /* namespace FN */