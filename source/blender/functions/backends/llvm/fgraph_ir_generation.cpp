#include "fgraph_ir_generation.hpp"
#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

	class BuildGraphIR : public LLVMBuildIRBody {
	private:
		FunctionGraph m_fgraph;
		SmallSocketSetVector m_inputs;
		SmallSocketSetVector m_outputs;
		SocketSet m_required_sockets;

		using SocketValueMap = SmallMap<Socket, llvm::Value *>;

	public:
		BuildGraphIR(FunctionGraph &fgraph)
			: m_fgraph(fgraph),
			  m_inputs(fgraph.inputs()),
			  m_outputs(fgraph.outputs())
		{
			for (Node *node : m_fgraph.graph()->all_nodes()) {
				SharedFunction &fn = node->function();
				if (fn->has_body<LLVMBuildIRBody>()) {
					continue;
				}
				if (fn->has_body<TupleCallBody>()) {
					derive_LLVMBuildIRBody_from_TupleCallBody(fn);
					continue;
				}
			}
			m_required_sockets = this->find_required_sockets();
		}

		void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &input_values,
			LLVMValues &r_output_values) const override
		{
			SocketValueMap values;
			for (uint i = 0; i < input_values.size(); i++) {
				values.add(m_inputs[i], input_values[i]);
			}

			SocketSet forwarded_sockets;
			for (Socket socket : m_outputs) {
				this->generate_for_socket(builder, socket, values, forwarded_sockets);
				r_output_values.append(values.lookup(socket));
			}
		}

	private:
		SocketSet find_required_sockets() const
		{
			SocketSet required_sockets;
			for (Socket socket : m_fgraph.outputs()) {
				this->find_required_sockets(socket, required_sockets);
			}
			return required_sockets;
		}

		void find_required_sockets(Socket socket, SocketSet &required_sockets) const
		{
			if (required_sockets.contains(socket)) {
				return;
			}

			required_sockets.add(socket);

			if (m_inputs.contains(socket)) {
				/* do nothing */
			}
			else if (socket.is_input()) {
				this->find_required_sockets(socket.origin(), required_sockets);
				return;
			}
			else if (socket.is_output()) {
				for (Socket input : socket.node()->inputs()) {
					this->find_required_sockets(input, required_sockets);
				}
				return;
			}
			else {
				BLI_assert(!"should never happen");
			}
		}

		void generate_for_socket(
			llvm::IRBuilder<> &builder,
			Socket socket,
			SocketValueMap &values,
			SocketSet &forwarded_sockets) const
		{
			if (values.contains(socket)) {
				/* do nothing */
			}
			else if (socket.is_input()) {
				Socket origin = socket.origin();
				this->generate_for_socket(builder, origin, values, forwarded_sockets);
				this->forward_output_if_necessary(builder, origin, values, forwarded_sockets);
			}
			else if (socket.is_output()) {
				Node *node = socket.node();
				LLVMValues input_values;
				for (Socket input : node->inputs()) {
					this->generate_for_socket(builder, input, values, forwarded_sockets);
					input_values.append(values.lookup(input));
				}

				LLVMValues output_values = this->build_node_ir(builder, node, input_values);

				for (uint i = 0; i < output_values.size(); i++) {
					Socket output = node->output(i);
					values.add(output, output_values[i]);
					this->forward_output_if_necessary(builder, output, values, forwarded_sockets);
				}
			}
			else {
				BLI_assert(!"should never happen");
			}

		}

		void forward_output_if_necessary(
			llvm::IRBuilder<> &builder,
			Socket output,
			SocketValueMap &values,
			SocketSet &forwarded_sockets) const
		{
			BLI_assert(output.is_output());
			if (!forwarded_sockets.contains(output)) {
				this->forward_output(builder, output, values);
				forwarded_sockets.add(output);
			}
		}

		void forward_output(
			llvm::IRBuilder<> &builder,
			Socket output,
			SocketValueMap &values) const
		{
			llvm::Value *value_to_forward = values.lookup(output);
			SharedType &type = output.type();
			LLVMTypeInfo *type_info = type->extension<LLVMTypeInfo>();
			BLI_assert(type_info);

			SmallSocketVector targets;
			for (Socket target : output.targets()) {
				if (m_required_sockets.contains(target) && !values.contains(target)) {
					BLI_assert(type == target.type());
					targets.append(target);
				}
			}

			if (targets.size() == 0) {
				type_info->build_free_ir(builder, value_to_forward);
			}
			else if (targets.size() == 1) {
				values.add(targets[0], value_to_forward);
			}
			else {
				values.add(targets[0], value_to_forward);
				for (uint i = 1; i < targets.size(); i++) {
					Socket target = targets[i];
					llvm::Value *copied_value = type_info->build_copy_ir(builder, value_to_forward);
					values.add(target, copied_value);
				}
			}
		}

		LLVMValues build_node_ir(
			llvm::IRBuilder<> &builder,
			Node *node,
			LLVMValues &input_values) const
		{
			LLVMValues output_values;
			SharedFunction &fn = node->function();
			if (fn->has_body<LLVMCompiledBody>()) {
				auto *body = fn->body<LLVMCompiledBody>();
				body->build_ir(builder, input_values, output_values);
			}
			else if (fn->has_body<LLVMBuildIRBody>()) {
				auto *body = fn->body<LLVMBuildIRBody>();
				body->build_ir(builder, input_values, output_values);
			}
			else {
				BLI_assert(false);
			}
			return output_values;
		}
	};

	void fgraph_add_LLVMBuildIRBody(
		SharedFunction &fn,
		FunctionGraph &fgraph)
	{
		fn->add_body(new BuildGraphIR(fgraph));
	}

} /* namespace FN */