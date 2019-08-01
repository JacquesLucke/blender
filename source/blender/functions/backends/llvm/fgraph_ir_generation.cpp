#include "fgraph_ir_generation.hpp"
#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

class BuildGraphIR : public LLVMBuildIRBody {
 private:
  FunctionGraph m_fgraph;
  DataGraph *m_graph;
  Set<DataSocket> m_required_sockets;

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
    Map<DataSocket, llvm::Value *> values;
    for (uint i = 0; i < m_fgraph.inputs().size(); i++) {
      values.add(m_fgraph.inputs()[i], interface.get_input(i));
    }

    Set<DataSocket> forwarded_sockets;
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DataSocket socket = m_fgraph.outputs()[i];
      this->generate_for_socket(builder, interface, settings, socket, values, forwarded_sockets);

      interface.set_output(i, values.lookup(socket));
    }
  }

 private:
  void generate_for_socket(CodeBuilder &builder,
                           CodeInterface &interface,
                           const BuildIRSettings &settings,
                           DataSocket socket,
                           Map<DataSocket, llvm::Value *> &values,
                           Set<DataSocket> &forwarded_sockets) const
  {
    if (values.contains(socket)) {
      /* do nothing */
    }
    else if (socket.is_input()) {
      DataSocket origin = m_graph->origin_of_input(socket);
      this->generate_for_socket(builder, interface, settings, origin, values, forwarded_sockets);
      this->forward_output_if_necessary(builder, origin, values, forwarded_sockets);
    }
    else if (socket.is_output()) {
      uint node_id = m_graph->node_id_of_output(socket);
      Vector<llvm::Value *> input_values;
      for (DataSocket input_socket : m_graph->inputs_of_node(node_id)) {
        this->generate_for_socket(
            builder, interface, settings, input_socket, values, forwarded_sockets);
        input_values.append(values.lookup(input_socket));
      }

      Vector<llvm::Value *> output_values = this->build_node_ir(
          builder, interface, settings, node_id, input_values);

      uint index = 0;
      for (DataSocket output_socket : m_graph->outputs_of_node(node_id)) {
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
                                   DataSocket output,
                                   Map<DataSocket, llvm::Value *> &values,
                                   Set<DataSocket> &forwarded_sockets) const
  {
    BLI_assert(output.is_output());
    if (!forwarded_sockets.contains(output)) {
      this->forward_output(builder, output, values);
      forwarded_sockets.add(output);
    }
  }

  void forward_output(CodeBuilder &builder,
                      DataSocket output,
                      Map<DataSocket, llvm::Value *> &values) const
  {
    llvm::Value *value_to_forward = values.lookup(output);
    SharedType &type = m_graph->type_of_socket(output);
    LLVMTypeInfo &type_info = type->extension<LLVMTypeInfo>();

    Vector<DataSocket> targets;
    for (DataSocket target : m_graph->targets_of_output(output)) {
      if (m_required_sockets.contains(target) && !values.contains(target)) {
        BLI_assert(type == m_graph->type_of_socket(target));
        targets.append(target);
      }
    }

    if (targets.size() == 0) {
      type_info.build_free_ir(builder, value_to_forward);
    }
    else if (targets.size() == 1) {
      values.add(targets[0], value_to_forward);
    }
    else {
      values.add(targets[0], value_to_forward);
      for (uint i = 1; i < targets.size(); i++) {
        DataSocket target = targets[i];
        llvm::Value *copied_value = type_info.build_copy_ir(builder, value_to_forward);
        values.add(target, copied_value);
      }
    }
  }

  Vector<llvm::Value *> build_node_ir(CodeBuilder &builder,
                                      CodeInterface &interface,
                                      const BuildIRSettings &settings,
                                      uint node_id,
                                      Vector<llvm::Value *> &input_values) const
  {
    SharedFunction &fn = m_graph->function_of_node(node_id);
    auto &body = fn->body<LLVMBuildIRBody>();
    bool setup_stack = settings.maintain_stack() && body.prepare_execution_context();

    if (setup_stack) {
      this->push_stack_frames_for_node(builder, interface.context_ptr(), node_id);
    }

    Vector<llvm::Value *> output_values(m_graph->outputs_of_node(node_id).size());
    CodeInterface sub_interface(
        input_values, output_values, interface.context_ptr(), interface.function_ir_cache());

    body.build_ir(builder, sub_interface, settings);

    if (setup_stack) {
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

    llvm::Value *node_info_frame_buf = builder.CreateAllocaBytes_AnyPtr(
        sizeof(SourceInfoStackFrame));
    llvm::Value *function_info_frame_buf = builder.CreateAllocaBytes_AnyPtr(
        sizeof(TextStackFrame));

    builder.CreateCallPointer((void *)BuildGraphIR::push_frames_on_stack,
                              {context_ptr,
                               node_info_frame_buf,
                               builder.getAnyPtr(source_info),
                               function_info_frame_buf,
                               builder.getAnyPtr(m_graph->name_ptr_of_node(node_id))},
                              builder.getVoidTy(),
                              "Push stack frames");
  }

  void pop_stack_frames_for_node(CodeBuilder &builder, llvm::Value *context_ptr) const
  {
    BLI_assert(context_ptr);
    builder.CreateCallPointer((void *)BuildGraphIR::pop_frames_from_stack,
                              {context_ptr},
                              builder.getVoidTy(),
                              "Pop stack frames");
  }

  static void push_frames_on_stack(ExecutionContext *ctx,
                                   void *source_frame_buf,
                                   SourceInfo *source_info,
                                   void *text_frame_buf,
                                   const char *text)
  {
    StackFrame *frame1 = new (source_frame_buf) SourceInfoStackFrame(source_info);
    StackFrame *frame2 = new (text_frame_buf) TextStackFrame(text);
    ctx->stack().push(frame1);
    ctx->stack().push(frame2);
  }

  static void pop_frames_from_stack(ExecutionContext *ctx)
  {
    ctx->stack().pop();
    ctx->stack().pop();
  }
};

void fgraph_add_LLVMBuildIRBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  fn->add_body<BuildGraphIR>(fgraph);
}

} /* namespace FN */
