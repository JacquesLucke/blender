#include "fgraph_ir_generation.hpp"
#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

	class BuildGraphIR : public LLVMBuildIRBody {
	private:
		FunctionGraph m_fgraph;
		SocketSetVector m_inputs;
		SocketSetVector m_outputs;
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
				if (fn->has_body<LazyInTupleCallBody>()) {
					derive_TupleCallBody_from_LazyInTupleCallBody(fn);
					derive_LLVMBuildIRBody_from_TupleCallBody(fn);
					continue;
				}
			}
			m_required_sockets = fgraph.find_required_sockets();
		}

		void build_ir(
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &settings) const override
		{
			SocketValueMap values;
			for (uint i = 0; i < interface.inputs().size(); i++) {
				values.add(m_inputs[i], interface.get_input(i));
			}

			SocketSet forwarded_sockets;
			for (uint i = 0; i < m_outputs.size(); i++) {
				Socket socket = m_outputs[i];
				this->generate_for_socket(
					builder, interface, settings, socket, values, forwarded_sockets);

				interface.set_output(i, values.lookup(socket));
			}
		}

	private:
		void generate_for_socket(
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &settings,
			Socket socket,
			SocketValueMap &values,
			SocketSet &forwarded_sockets) const
		{
			if (values.contains(socket)) {
				/* do nothing */
			}
			else if (socket.is_input()) {
				Socket origin = socket.origin();
				this->generate_for_socket(
					builder, interface, settings, origin, values, forwarded_sockets);
				this->forward_output_if_necessary(builder, origin, values, forwarded_sockets);
			}
			else if (socket.is_output()) {
				Node *node = socket.node();
				LLVMValues input_values;
				for (Socket input : node->inputs()) {
					this->generate_for_socket(
						builder, interface, settings, input, values, forwarded_sockets);
					input_values.append(values.lookup(input));
				}

				LLVMValues output_values = this->build_node_ir(
					builder, interface, settings, node, input_values);

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
			CodeBuilder &builder,
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
			CodeBuilder &builder,
			Socket output,
			SocketValueMap &values) const
		{
			llvm::Value *value_to_forward = values.lookup(output);
			SharedType &type = output.type();
			LLVMTypeInfo *type_info = type->extension<LLVMTypeInfo>();
			BLI_assert(type_info);

			SocketVector targets;
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
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &settings,
			Node *node,
			LLVMValues &input_values) const
		{
			if (settings.maintain_stack()) {
				this->push_stack_frames_for_node(builder, interface.context_ptr(), node);
			}

			SharedFunction &fn = node->function();
			LLVMValues output_values(node->output_amount());
			CodeInterface sub_interface(
				input_values, output_values,
				interface.context_ptr(), interface.function_ir_cache());

			BLI_assert(fn->has_body<LLVMBuildIRBody>());
			auto *body = fn->body<LLVMBuildIRBody>();
			body->build_ir(builder, sub_interface, settings);

			if (settings.maintain_stack()) {
				this->pop_stack_frames_for_node(builder, interface.context_ptr());
			}

			return output_values;
		}

		void push_stack_frames_for_node(
			CodeBuilder &builder,
			llvm::Value *context_ptr,
			Node *node) const
		{
			BLI_assert(context_ptr);

			llvm::Value *node_info_frame_buf = builder.CreateAllocaBytes_VoidPtr(sizeof(SourceInfoStackFrame));
			builder.CreateCallPointer_NoReturnValue(
				(void *)BuildGraphIR::push_source_frame_on_stack,
				{ context_ptr,
				  node_info_frame_buf,
				  builder.getVoidPtr(node) });

			llvm::Value *function_info_frame_buf = builder.CreateAllocaBytes_VoidPtr(sizeof(TextStackFrame));
			builder.CreateCallPointer_NoReturnValue(
				(void *)BuildGraphIR::push_text_frame_on_stack,
				{ context_ptr,
				  function_info_frame_buf,
				  builder.getVoidPtr((void *)node->function()->name().c_str())});
		}

		void pop_stack_frames_for_node(
			CodeBuilder &builder,
			llvm::Value *context_ptr) const
		{
			BLI_assert(context_ptr);

			for (uint i = 0; i < 2; i++) {
				builder.CreateCallPointer_NoReturnValue(
					(void *)BuildGraphIR::pop_frame_from_stack,
					{ context_ptr });
			}
		}

		static void push_source_frame_on_stack(ExecutionContext *ctx, void *frame_buf, Node *node)
		{
			StackFrame *frame = new(frame_buf) SourceInfoStackFrame(node->source());
			ctx->stack().push(frame);
		}

		static void push_text_frame_on_stack(ExecutionContext *ctx, void *frame_buf, const char *text)
		{
			StackFrame *frame = new(frame_buf) TextStackFrame(text);
			ctx->stack().push(frame);
		}

		static void pop_frame_from_stack(ExecutionContext *ctx)
		{
			ctx->stack().pop();
		}
	};

	void fgraph_add_LLVMBuildIRBody(
		SharedFunction &fn,
		FunctionGraph &fgraph)
	{
		fn->add_body(new BuildGraphIR(fgraph));
	}

} /* namespace FN */
