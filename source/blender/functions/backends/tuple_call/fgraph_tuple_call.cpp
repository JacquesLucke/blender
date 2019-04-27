#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

static void try_ensure_tuple_call_bodies(SharedDataFlowGraph &graph)
{
  auto *context = new llvm::LLVMContext();

  for (uint node_id : graph->node_ids()) {
    SharedFunction &fn = graph->function_of_node(node_id);
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

class ExecuteFGraph : public TupleCallBody {
 private:
  FunctionGraph m_fgraph;
  DataFlowGraph *m_graph;

  SmallVector<TupleCallBody *> m_bodies;
  SmallVector<uint> m_input_starts;
  SmallVector<uint> m_output_starts;

  SmallVector<CPPTypeInfo *> m_input_types;
  SmallVector<CPPTypeInfo *> m_output_types;
  SmallVector<uint> m_input_offsets;
  SmallVector<uint> m_output_offsets;

  uint m_inputs_buffer_size = 0;
  uint m_outputs_buffer_size = 0;
  uint m_inputs_init_buffer_size = 0;
  uint m_outputs_init_buffer_size = 0;

  struct SocketFlag {
    char is_fn_input : 1;
    char is_fn_output : 1;
  };

  SmallVector<SocketFlag> m_input_socket_flags;
  SmallVector<SocketFlag> m_output_socket_flags;

 public:
  ExecuteFGraph(FunctionGraph &fgraph) : m_fgraph(fgraph), m_graph(fgraph.graph().ptr())
  {
    for (uint node_id : m_graph->node_ids()) {
      SharedFunction &fn = m_graph->function_of_node(node_id);
      TupleCallBody *body = fn->body<TupleCallBody>();
      m_bodies.append(body);

      m_inputs_init_buffer_size += fn->signature().inputs().size();
      m_outputs_init_buffer_size += fn->signature().outputs().size();

      m_input_starts.append(m_inputs_buffer_size);
      m_output_starts.append(m_outputs_buffer_size);

      for (auto socket : m_graph->inputs_of_node(node_id)) {
        SocketFlag flag;
        flag.is_fn_input = fgraph.inputs().contains(socket);
        flag.is_fn_output = fgraph.outputs().contains(socket);
        m_input_socket_flags.append(flag);
      }

      for (auto socket : m_graph->outputs_of_node(node_id)) {
        SocketFlag flag;
        flag.is_fn_input = fgraph.inputs().contains(socket);
        flag.is_fn_output = fgraph.outputs().contains(socket);
        m_output_socket_flags.append(flag);
      }

      if (body == nullptr) {
        for (auto param : fn->signature().inputs()) {
          CPPTypeInfo *type_info = param.type()->extension<CPPTypeInfo>();
          BLI_assert(type_info);
          uint type_size = type_info->size_of_type();

          m_input_types.append(type_info);
          m_input_offsets.append(m_inputs_buffer_size);
          m_inputs_buffer_size += type_size;
        }

        for (auto param : fn->signature().outputs()) {
          CPPTypeInfo *type_info = param.type()->extension<CPPTypeInfo>();
          BLI_assert(type_info);
          uint type_size = type_info->size_of_type();

          m_output_types.append(type_info);
          m_output_offsets.append(m_outputs_buffer_size);
          m_outputs_buffer_size += type_size;
        }
      }
      else {
        SharedTupleMeta &meta_in = body->meta_in();
        for (uint i = 0; i < fn->signature().inputs().size(); i++) {
          m_input_types.append(meta_in->type_infos()[i]);
          m_input_offsets.append(m_inputs_buffer_size + meta_in->offsets()[i]);
        }
        m_inputs_buffer_size += meta_in->size_of_data();

        SharedTupleMeta &meta_out = body->meta_out();
        for (uint i = 0; i < fn->signature().outputs().size(); i++) {
          m_output_types.append(meta_out->type_infos()[i]);
          m_output_offsets.append(m_outputs_buffer_size + meta_out->offsets()[i]);
        }
        m_outputs_buffer_size += meta_out->size_of_data();
      }
    }
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    char *input_values = BLI_array_alloca(input_values, m_inputs_buffer_size);
    char *output_values = BLI_array_alloca(output_values, m_outputs_buffer_size);

    bool *input_inits = BLI_array_alloca(input_inits, m_inputs_init_buffer_size);
    bool *output_inits = BLI_array_alloca(output_inits, m_outputs_init_buffer_size);
    memset(input_inits, 0, m_inputs_init_buffer_size);
    memset(output_inits, 0, m_outputs_init_buffer_size);

    for (uint i = 0; i < m_fgraph.inputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.inputs()[i];
      if (socket.is_input()) {
        fn_in.relocate_out__dynamic(i, input_values + m_input_offsets[socket.id()]);
        input_inits[socket.id()] = true;

        if (m_input_socket_flags[socket.id()].is_fn_output) {
          uint index = m_fgraph.outputs().index(socket);
          fn_out.copy_in__dynamic(index, input_values + m_input_offsets[socket.id()]);
        }
      }
      else {
        fn_in.relocate_out__dynamic(i, output_values + m_output_offsets[socket.id()]);
        output_inits[socket.id()] = true;

        if (m_output_socket_flags[socket.id()].is_fn_output) {
          uint index = m_fgraph.outputs().index(socket);
          fn_out.copy_in__dynamic(index, output_values + m_output_offsets[socket.id()]);
        }
      }
    }

    SmallStack<DFGraphSocket> sockets_to_compute;
    for (auto socket : m_fgraph.outputs()) {
      sockets_to_compute.push(socket);
    }

    while (!sockets_to_compute.empty()) {
      DFGraphSocket socket = sockets_to_compute.peek();

      if (socket.is_input()) {
        if (input_inits[socket.id()]) {
          sockets_to_compute.pop();
        }
        else {
          DFGraphSocket origin = m_graph->origin_of_input(socket);
          if (output_inits[origin.id()]) {
            this->forward_output(
                origin.id(), input_values, output_values, input_inits, output_inits, fn_out);
            sockets_to_compute.pop();
          }
          else {
            sockets_to_compute.push(origin);
          }
        }
      }
      else {
        if (output_inits[socket.id()]) {
          sockets_to_compute.pop();
        }
        else {
          bool all_inputs_computed = true;
          uint node_id = m_graph->node_id_of_output(socket.id());
          for (uint input_id : m_graph->input_ids_of_node(node_id)) {
            if (!input_inits[input_id]) {
              sockets_to_compute.push(DFGraphSocket::FromInput(input_id));
              all_inputs_computed = false;
            }
          }

          if (all_inputs_computed) {
            TupleCallBody *body = m_bodies[node_id];
            BLI_assert(body);

            Tuple body_in(body->meta_in(),
                          input_values + m_input_starts[node_id],
                          input_inits + m_graph->first_input_id_of_node(node_id),
                          true,
                          true);
            Tuple body_out(body->meta_out(),
                           output_values + m_output_starts[node_id],
                           output_inits + m_graph->first_output_id_of_node(node_id),
                           true,
                           false);

            SourceInfoStackFrame frame(m_graph->source_info_of_node(node_id));
            body->call__setup_stack(body_in, body_out, ctx, frame);

            for (uint output_id : m_graph->output_ids_of_node(node_id)) {
              if (m_output_socket_flags[output_id].is_fn_output) {
                uint index = m_fgraph.outputs().index(DFGraphSocket::FromOutput(output_id));
                fn_out.copy_in__dynamic(index, output_values + m_output_offsets[output_id]);
              }
            }

            sockets_to_compute.pop();
          }
        }
      }
    }

    for (uint input_id = 0; input_id < m_inputs_init_buffer_size; input_id++) {
      if (input_inits[input_id]) {
        CPPTypeInfo *type_info = m_input_types[input_id];
        type_info->destruct_type(input_values + m_input_offsets[input_id]);
      }
    }
    for (uint output_id = 0; output_id < m_outputs_init_buffer_size; output_id++) {
      if (output_inits[output_id]) {
        CPPTypeInfo *type_info = m_output_types[output_id];
        type_info->destruct_type(output_values + m_output_offsets[output_id]);
      }
    }
  }

 private:
  void forward_output(uint output_id,
                      char *input_values,
                      char *output_values,
                      bool *input_inits,
                      bool *output_inits,
                      Tuple &fn_out) const
  {
    BLI_assert(output_inits[output_id]);
    auto target_ids = m_graph->targets_of_output(output_id);
    CPPTypeInfo *type_info = m_output_types[output_id];
    void *value_src = output_values + m_output_offsets[output_id];

    for (uint target_id : target_ids) {
      BLI_assert(type_info == m_input_types[target_id]);
      if (!input_inits[target_id]) {
        void *value_dst = input_values + m_input_offsets[target_id];
        type_info->copy_to_uninitialized(value_src, value_dst);
        input_inits[target_id] = true;

        if (m_input_socket_flags[target_id].is_fn_output) {
          uint index = m_fgraph.outputs().index(DFGraphSocket::FromInput(target_id));
          fn_out.copy_in__dynamic(index, value_dst);
        }
      }
    }
  }
};

class ExecuteFGraph_Simple : public TupleCallBody {
 private:
  FunctionGraph m_fgraph;
  /* Just for easy access. */
  DataFlowGraph *m_graph;

 public:
  ExecuteFGraph_Simple(FunctionGraph &function_graph)
      : m_fgraph(function_graph), m_graph(function_graph.graph().ptr())
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.outputs()[i];
      this->compute_socket(fn_in, fn_out, i, socket, ctx);
    }
  }

  void compute_socket(
      Tuple &fn_in, Tuple &out, uint out_index, DFGraphSocket socket, ExecutionContext &ctx) const
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
      for (DFGraphSocket input_socket : m_graph->inputs_of_node(node_id)) {
        this->compute_socket(fn_in, tmp_in, index, input_socket, ctx);
        index++;
      }

      SourceInfoStackFrame node_frame(m_graph->source_info_of_node(node_id));
      body->call__setup_stack(tmp_in, tmp_out, ctx, node_frame);

      Tuple::copy_element(tmp_out, m_graph->index_of_output(socket.id()), out, out_index);
    }
  }
};

void fgraph_add_TupleCallBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  try_ensure_tuple_call_bodies(fgraph.graph());
  fn->add_body(new ExecuteFGraph(fgraph));
}

} /* namespace FN */
