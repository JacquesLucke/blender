#include "fgraph_ir_generation.hpp"
#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

class BuildGraphIR : public LLVMBuildIRBody {
 private:
  FunctionGraph m_fgraph;
  DataFlowGraph *m_graph;
  Set<DFGraphSocket> m_required_sockets;

  using SocketValueMap = Map<DFGraphSocket, llvm::Value *>;
  using FunctionDFGB_SocketSet = Set<DFGraphSocket>;

 public:
  BuildGraphIR(FunctionGraph &fgraph) : m_fgraph(fgraph), m_graph(fgraph.graph().ptr())
  {
    for (uint node_id : m_graph->node_ids()) {
      SharedFunction &fn = m_fgraph.graph()->function_of_node(node_id);
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
    m_required_sockets = fgraph.find_used_sockets(false, true);
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &settings) const override
  {
    SocketValueMap values;
    for (uint i = 0; i < m_fgraph.inputs().size(); i++) {
      values.add(m_fgraph.inputs()[i], interface.get_input(i));
    }

    FunctionDFGB_SocketSet forwarded_sockets;
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.outputs()[i];
      this->generate_for_socket(builder, interface, settings, socket, values, forwarded_sockets);

      interface.set_output(i, values.lookup(socket));
    }
  }

 private:
  void generate_for_socket(CodeBuilder &builder,
                           CodeInterface &interface,
                           const BuildIRSettings &settings,
                           DFGraphSocket socket,
                           SocketValueMap &values,
                           FunctionDFGB_SocketSet &forwarded_sockets) const
  {
    if (values.contains(socket)) {
      /* do nothing */
    }
    else if (socket.is_input()) {
      DFGraphSocket origin = m_graph->origin_of_input(socket);
      this->generate_for_socket(builder, interface, settings, origin, values, forwarded_sockets);
      this->forward_output_if_necessary(builder, origin, values, forwarded_sockets);
    }
    else if (socket.is_output()) {
      uint node_id = m_graph->node_id_of_output(socket);
      LLVMValues input_values;
      for (DFGraphSocket input_socket : m_graph->inputs_of_node(node_id)) {
        this->generate_for_socket(
            builder, interface, settings, input_socket, values, forwarded_sockets);
        input_values.append(values.lookup(input_socket));
      }

      LLVMValues output_values = this->build_node_ir(
          builder, interface, settings, node_id, input_values);

      uint index = 0;
      for (DFGraphSocket output_socket : m_graph->outputs_of_node(node_id)) {
        values.add(output_socket, output_values[index]);
        this->forward_output_if_necessary(builder, output_socket, values, forwarded_sockets);
        index++;
      }
    }
    else {
      BLI_assert(!"should never happen");
    }
  }

  void forward_output_if_necessary(CodeBuilder &builder,
                                   DFGraphSocket output,
                                   SocketValueMap &values,
                                   FunctionDFGB_SocketSet &forwarded_sockets) const
  {
    BLI_assert(output.is_output());
    if (!forwarded_sockets.contains(output)) {
      this->forward_output(builder, output, values);
      forwarded_sockets.add(output);
    }
  }

  void forward_output(CodeBuilder &builder, DFGraphSocket output, SocketValueMap &values) const
  {
    llvm::Value *value_to_forward = values.lookup(output);
    SharedType &type = m_graph->type_of_socket(output);
    LLVMTypeInfo *type_info = type->extension<LLVMTypeInfo>();
    BLI_assert(type_info);

    Vector<DFGraphSocket> targets;
    for (DFGraphSocket target : m_graph->targets_of_output(output)) {
      if (m_required_sockets.contains(target) && !values.contains(target)) {
        BLI_assert(type == m_graph->type_of_socket(target));
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
        DFGraphSocket target = targets[i];
        llvm::Value *copied_value = type_info->build_copy_ir(builder, value_to_forward);
        values.add(target, copied_value);
      }
    }
  }

  LLVMValues build_node_ir(CodeBuilder &builder,
                           CodeInterface &interface,
                           const BuildIRSettings &settings,
                           uint node_id,
                           LLVMValues &input_values) const
  {
    if (settings.maintain_stack()) {
      this->push_stack_frames_for_node(builder, interface.context_ptr(), node_id);
    }

    SharedFunction &fn = m_graph->function_of_node(node_id);
    LLVMValues output_values(m_graph->outputs_of_node(node_id).size());
    CodeInterface sub_interface(
        input_values, output_values, interface.context_ptr(), interface.function_ir_cache());

    BLI_assert(fn->has_body<LLVMBuildIRBody>());
    auto *body = fn->body<LLVMBuildIRBody>();
    body->build_ir(builder, sub_interface, settings);

    if (settings.maintain_stack()) {
      this->pop_stack_frames_for_node(builder, interface.context_ptr());
    }

    return output_values;
  }

  void push_stack_frames_for_node(CodeBuilder &builder,
                                  llvm::Value *context_ptr,
                                  uint node_id) const
  {
    BLI_assert(context_ptr);
    SourceInfo *source_info = m_graph->source_info_of_node(node_id);

    llvm::Value *node_info_frame_buf = builder.CreateAllocaBytes_VoidPtr(
        sizeof(SourceInfoStackFrame));
    builder.CreateCallPointer(
        (void *)BuildGraphIR::push_source_frame_on_stack,
        {context_ptr, node_info_frame_buf, builder.getVoidPtr((void *)source_info)},
        builder.getVoidTy(),
        "Push source info on stack");

    llvm::Value *function_info_frame_buf = builder.CreateAllocaBytes_VoidPtr(
        sizeof(TextStackFrame));
    builder.CreateCallPointer((void *)BuildGraphIR::push_text_frame_on_stack,
                              {context_ptr,
                               function_info_frame_buf,
                               builder.getVoidPtr((void *)m_graph->name_ptr_of_node(node_id))},
                              builder.getVoidTy(),
                              "Push function name on stack");
  }

  void pop_stack_frames_for_node(CodeBuilder &builder, llvm::Value *context_ptr) const
  {
    BLI_assert(context_ptr);

    for (uint i = 0; i < 2; i++) {
      builder.CreateCallPointer((void *)BuildGraphIR::pop_frame_from_stack,
                                {context_ptr},
                                builder.getVoidTy(),
                                "Pop stack frame");
    }
  }

  static void push_source_frame_on_stack(ExecutionContext *ctx,
                                         void *frame_buf,
                                         SourceInfo *source_info)
  {
    StackFrame *frame = new (frame_buf) SourceInfoStackFrame(source_info);
    ctx->stack().push(frame);
  }

  static void push_text_frame_on_stack(ExecutionContext *ctx, void *frame_buf, const char *text)
  {
    StackFrame *frame = new (frame_buf) TextStackFrame(text);
    ctx->stack().push(frame);
  }

  static void pop_frame_from_stack(ExecutionContext *ctx)
  {
    ctx->stack().pop();
  }
};

void fgraph_add_LLVMBuildIRBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  fn->add_body<BuildGraphIR>(fgraph);
}

} /* namespace FN */
