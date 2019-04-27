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

  struct SocketInfo {
    CPPTypeInfo *type;
    uint offset;
    uint is_fn_output : 1;

    SocketInfo(CPPTypeInfo *type, uint offset, bool is_fn_output)
        : type(type), offset(offset), is_fn_output(is_fn_output)
    {
    }
  };

  SmallVector<SocketInfo> m_input_info;
  SmallVector<SocketInfo> m_output_info;

  uint m_inputs_buffer_size = 0;
  uint m_outputs_buffer_size = 0;
  uint m_inputs_init_buffer_size = 0;
  uint m_outputs_init_buffer_size = 0;

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

      if (body == nullptr) {
        for (auto param : fn->signature().inputs()) {
          CPPTypeInfo *type_info = param.type()->extension<CPPTypeInfo>();
          BLI_assert(type_info);
          uint type_size = type_info->size_of_type();
          m_input_info.append(SocketInfo(type_info, m_inputs_init_buffer_size, false));
          m_inputs_buffer_size += type_size;
        }

        for (auto param : fn->signature().outputs()) {
          CPPTypeInfo *type_info = param.type()->extension<CPPTypeInfo>();
          BLI_assert(type_info);
          uint type_size = type_info->size_of_type();
          m_output_info.append(SocketInfo(type_info, m_outputs_buffer_size, false));
          m_outputs_buffer_size += type_size;
        }
      }
      else {
        SharedTupleMeta &meta_in = body->meta_in();
        for (uint i = 0; i < fn->signature().inputs().size(); i++) {
          m_input_info.append(SocketInfo(
              meta_in->type_infos()[i], m_inputs_buffer_size + meta_in->offsets()[i], false));
        }
        m_inputs_buffer_size += meta_in->size_of_data();

        SharedTupleMeta &meta_out = body->meta_out();
        for (uint i = 0; i < fn->signature().outputs().size(); i++) {
          m_output_info.append(SocketInfo(
              meta_out->type_infos()[i], m_outputs_buffer_size + meta_out->offsets()[i], false));
        }
        m_outputs_buffer_size += meta_out->size_of_data();
      }
    }

    for (auto socket : m_fgraph.outputs()) {
      if (socket.is_input()) {
        m_input_info[socket.id()].is_fn_output = true;
      }
      else {
        m_output_info[socket.id()].is_fn_output = true;
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
        SocketInfo &socket_info = m_input_info[socket.id()];
        fn_in.relocate_out__dynamic(i, input_values + socket_info.offset);
        input_inits[socket.id()] = true;

        if (socket_info.is_fn_output) {
          uint index = m_fgraph.outputs().index(socket);
          fn_out.copy_in__dynamic(index, input_values + socket_info.offset);
        }
      }
      else {
        SocketInfo &socket_info = m_output_info[socket.id()];
        fn_in.relocate_out__dynamic(i, output_values + socket_info.offset);
        output_inits[socket.id()] = true;

        if (socket_info.is_fn_output) {
          uint index = m_fgraph.outputs().index(socket);
          fn_out.copy_in__dynamic(index, output_values + socket_info.offset);
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
              SocketInfo &socket_info = m_output_info[output_id];
              if (socket_info.is_fn_output) {
                uint index = m_fgraph.outputs().index(DFGraphSocket::FromOutput(output_id));
                fn_out.copy_in__dynamic(index, output_values + socket_info.offset);
              }
            }

            sockets_to_compute.pop();
          }
        }
      }
    }

    for (uint input_id = 0; input_id < m_inputs_init_buffer_size; input_id++) {
      if (input_inits[input_id]) {
        SocketInfo &socket_info = m_input_info[input_id];
        socket_info.type->destruct_type(input_values + socket_info.offset);
      }
    }
    for (uint output_id = 0; output_id < m_outputs_init_buffer_size; output_id++) {
      if (output_inits[output_id]) {
        SocketInfo &socket_info = m_output_info[output_id];
        socket_info.type->destruct_type(output_values + socket_info.offset);
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
    auto possible_target_ids = m_graph->targets_of_output(output_id);

    SocketInfo &output_info = m_output_info[output_id];
    CPPTypeInfo *type_info = output_info.type;
    void *value_src = output_values + output_info.offset;

    uint *target_ids = BLI_array_alloca(target_ids, possible_target_ids.size());
    uint target_amount = 0;
    for (uint possible_target_id : possible_target_ids) {
      if (!input_inits[possible_target_id]) {
        target_ids[target_amount] = possible_target_id;
        target_amount++;
      }
    }

    if (target_amount == 0) {
      type_info->destruct_type(value_src);
      output_inits[output_id] = false;
    }
    else if (target_amount == 1) {
      uint target_id = target_ids[0];
      void *value_dst = input_values + m_input_info[target_id].offset;
      type_info->relocate_to_uninitialized(value_src, value_dst);
      output_inits[output_id] = false;
      input_inits[target_id] = true;
    }
    else {
      for (uint i = 1; i < target_amount; i++) {
        uint target_id = target_ids[i];
        void *value_dst = input_values + m_input_info[target_id].offset;
        type_info->copy_to_uninitialized(value_src, value_dst);
        input_inits[target_id] = true;
      }

      uint target_id = target_ids[0];
      void *value_dst = input_values + m_input_info[target_id].offset;
      type_info->copy_to_uninitialized(value_src, value_dst);
      output_inits[output_id] = false;
      input_inits[target_id] = true;
    }

    for (uint i = 0; i < target_amount; i++) {
      uint target_id = target_ids[i];
      SocketInfo &socket_info = m_input_info[target_id];
      BLI_assert(type_info == socket_info.type);
      if (socket_info.is_fn_output) {
        uint index = m_fgraph.outputs().index(DFGraphSocket::FromInput(target_id));
        void *value_ptr = input_values + socket_info.offset;
        fn_out.copy_in__dynamic(index, value_ptr);
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
