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

  struct NodeInfo {
    TupleCallBodyBase *body;
    uint input_start;
    uint output_start;
    bool is_lazy;

    NodeInfo(TupleCallBodyBase *body, bool is_lazy, uint input_start, uint output_start)
        : body(body), input_start(input_start), output_start(output_start), is_lazy(is_lazy)
    {
    }
  };

  SmallVector<NodeInfo> m_node_info;

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

      TupleCallBodyBase *body;
      bool is_lazy_body;
      if (fn->has_body<LazyInTupleCallBody>()) {
        body = fn->body<LazyInTupleCallBody>();
        is_lazy_body = true;
      }
      else {
        body = fn->body<TupleCallBody>();
        is_lazy_body = false;
      }

      m_node_info.append(
          NodeInfo(body, is_lazy_body, m_inputs_buffer_size, m_outputs_buffer_size));

      m_inputs_init_buffer_size += fn->signature().inputs().size();
      m_outputs_init_buffer_size += fn->signature().outputs().size();

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

  struct SocketValueStorage {
    const ExecuteFGraph &m_parent;
    void *m_input_values;
    void *m_output_values;
    bool *m_input_inits;
    bool *m_output_inits;

    SocketValueStorage(const ExecuteFGraph &parent) : m_parent(parent)
    {
    }

    void *input_value_ptr(uint input_socket_id)
    {
      return POINTER_OFFSET(m_input_values, m_parent.m_input_info[input_socket_id].offset);
    }

    void *output_value_ptr(uint output_socket_id)
    {
      return POINTER_OFFSET(m_output_values, m_parent.m_output_info[output_socket_id].offset);
    }

    void *node_input_values_ptr(uint node_id)
    {
      return POINTER_OFFSET(m_input_values, m_parent.m_node_info[node_id].input_start);
    }

    void *node_output_values_ptr(uint node_id)
    {
      return POINTER_OFFSET(m_output_values, m_parent.m_node_info[node_id].output_start);
    }

    bool *node_input_inits_ptr(uint node_id)
    {
      return (bool *)POINTER_OFFSET(m_input_inits,
                                    m_parent.m_graph->first_input_id_of_node(node_id));
    }

    bool *node_output_inits_ptr(uint node_id)
    {
      return (bool *)POINTER_OFFSET(m_output_inits,
                                    m_parent.m_graph->first_output_id_of_node(node_id));
    }

    bool is_input_initialized(uint input_socket_id)
    {
      return m_input_inits[input_socket_id];
    }

    bool is_output_initialized(uint output_socket_id)
    {
      return m_output_inits[output_socket_id];
    }

    void set_input_initialized(uint input_socket_id, bool is_initialized)
    {
      m_input_inits[input_socket_id] = is_initialized;
    }

    void set_output_initialized(uint output_socket_id, bool is_initialized)
    {
      m_output_inits[output_socket_id] = is_initialized;
    }
  };

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    SocketValueStorage storage(*this);
    storage.m_input_values = alloca(m_inputs_buffer_size);
    storage.m_output_values = alloca(m_outputs_buffer_size);
    storage.m_input_inits = (bool *)alloca(m_inputs_init_buffer_size);
    storage.m_output_inits = (bool *)alloca(m_outputs_init_buffer_size);
    memset(storage.m_input_inits, 0, m_inputs_init_buffer_size);
    memset(storage.m_output_inits, 0, m_outputs_init_buffer_size);

    this->copy_inputs_to_storage(fn_in, fn_out, storage);
    this->evaluate_graph_to_compute_outputs(storage, fn_out, ctx);
    this->destruct_remaining_values(storage);
  }

 private:
  void copy_inputs_to_storage(Tuple &fn_in, Tuple &fn_out, SocketValueStorage &storage) const
  {
    for (uint i = 0; i < m_fgraph.inputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.inputs()[i];
      if (socket.is_input()) {
        fn_in.relocate_out__dynamic(i, storage.input_value_ptr(socket.id()));
        storage.set_input_initialized(socket.id(), true);

        if (m_input_info[socket.id()].is_fn_output) {
          uint index = m_fgraph.outputs().index(socket);
          fn_out.copy_in__dynamic(index, storage.input_value_ptr(socket.id()));
        }
      }
      else {
        fn_in.relocate_out__dynamic(i, storage.output_value_ptr(socket.id()));
        storage.set_output_initialized(socket.id(), true);

        if (m_output_info[socket.id()].is_fn_output) {
          uint index = m_fgraph.outputs().index(socket);
          fn_out.copy_in__dynamic(index, storage.output_value_ptr(socket.id()));
        }
      }
    }
  }

  struct LazyStateOnStack {
    uint node_id;
    LazyState state;

    LazyStateOnStack(uint node_id, LazyState state) : node_id(node_id), state(state)
    {
    }
  };

  void evaluate_graph_to_compute_outputs(SocketValueStorage &storage,
                                         Tuple &fn_out,
                                         ExecutionContext &ctx) const
  {
    SmallStack<DFGraphSocket, 64> sockets_to_compute;
    SmallStack<LazyStateOnStack> lazy_states;

    for (auto socket : m_fgraph.outputs()) {
      sockets_to_compute.push(socket);
    }

    while (!sockets_to_compute.empty()) {
      DFGraphSocket socket = sockets_to_compute.peek();

      if (socket.is_input()) {
        if (storage.is_input_initialized(socket.id())) {
          sockets_to_compute.pop();
        }
        else {
          DFGraphSocket origin = m_graph->origin_of_input(socket);
          if (storage.is_output_initialized(origin.id())) {
            this->forward_output(origin.id(), storage, fn_out);
            sockets_to_compute.pop();
          }
          else {
            sockets_to_compute.push(origin);
          }
        }
      }
      else {
        if (storage.is_output_initialized(socket.id())) {
          sockets_to_compute.pop();
        }
        else {
          uint node_id = m_graph->node_id_of_output(socket.id());

          if (m_node_info[node_id].is_lazy) {
            LazyInTupleCallBody *body = (LazyInTupleCallBody *)m_node_info[node_id].body;

            Tuple body_in(body->meta_in(),
                          storage.node_input_values_ptr(node_id),
                          storage.node_input_inits_ptr(node_id),
                          true,
                          false);
            Tuple body_out(body->meta_out(),
                           storage.node_output_values_ptr(node_id),
                           storage.node_output_inits_ptr(node_id),
                           true,
                           false);

            if (lazy_states.empty() || lazy_states.peek().node_id != node_id) {

              bool required_inputs_computed = true;

              for (uint input_index : body->always_required()) {
                uint input_id = m_graph->id_of_node_input(node_id, input_index);
                if (!storage.is_input_initialized(input_id)) {
                  sockets_to_compute.push(DFGraphSocket::FromInput(input_id));
                  required_inputs_computed = false;
                }
              }

              if (required_inputs_computed) {
                void *user_data = alloca(body->user_data_size());
                LazyState state(user_data);
                state.start_next_entry();

                SourceInfoStackFrame frame(m_graph->source_info_of_node(node_id));
                ctx.stack().push(&frame);
                body->call(body_in, body_out, ctx, state);
                ctx.stack().pop();

                if (state.is_done()) {
                  this->copy_outputs_to_final_output_if_necessary(node_id, storage, fn_out);
                  sockets_to_compute.pop();
                }
                else {
                  for (uint requested_input_index : state.requested_inputs()) {
                    uint input_id = m_graph->id_of_node_input(node_id, requested_input_index);
                    if (!storage.is_input_initialized(input_id)) {
                      sockets_to_compute.push(DFGraphSocket::FromInput(input_id));
                    }
                  }
                  lazy_states.push(LazyStateOnStack(node_id, state));
                }
              }
            }
            else {
              LazyState &state = lazy_states.peek().state;
              state.start_next_entry();

              SourceInfoStackFrame frame(m_graph->source_info_of_node(node_id));
              ctx.stack().push(&frame);
              body->call(body_in, body_out, ctx, state);
              ctx.stack().pop();

              if (state.is_done()) {
                this->copy_outputs_to_final_output_if_necessary(node_id, storage, fn_out);
                sockets_to_compute.pop();
                lazy_states.pop();

                // TODO: destruct inputs
              }
              else {
                for (uint requested_input_index : state.requested_inputs()) {
                  uint input_id = m_graph->id_of_node_input(node_id, requested_input_index);
                  if (!storage.is_input_initialized(input_id)) {
                    sockets_to_compute.push(DFGraphSocket::FromInput(input_id));
                  }
                }
              }
            }
          }
          else {
            bool all_inputs_computed = true;

            for (uint input_id : m_graph->input_ids_of_node(node_id)) {
              if (!storage.is_input_initialized(input_id)) {
                sockets_to_compute.push(DFGraphSocket::FromInput(input_id));
                all_inputs_computed = false;
              }
            }

            if (all_inputs_computed) {
              BLI_assert(!m_node_info[node_id].is_lazy);
              TupleCallBody *body = (TupleCallBody *)m_node_info[node_id].body;
              BLI_assert(body);

              Tuple body_in(body->meta_in(),
                            storage.node_input_values_ptr(node_id),
                            storage.node_input_inits_ptr(node_id),
                            true,
                            true);
              Tuple body_out(body->meta_out(),
                             storage.node_output_values_ptr(node_id),
                             storage.node_output_inits_ptr(node_id),
                             true,
                             false);

              SourceInfo *source_info = m_graph->source_info_of_node(node_id);
              body->call__setup_stack(body_in, body_out, ctx, source_info);
              BLI_assert(body_out.all_initialized());

              this->copy_outputs_to_final_output_if_necessary(node_id, storage, fn_out);
              sockets_to_compute.pop();
            }
          }
        }
      }
    }
  }

  void copy_outputs_to_final_output_if_necessary(uint node_id,
                                                 SocketValueStorage &storage,
                                                 Tuple &fn_out) const
  {
    for (uint output_id : m_graph->output_ids_of_node(node_id)) {
      if (m_output_info[output_id].is_fn_output) {
        uint index = m_fgraph.outputs().index(DFGraphSocket::FromOutput(output_id));
        fn_out.copy_in__dynamic(index, storage.output_value_ptr(output_id));
      }
    }
  }

  void forward_output(uint output_id, SocketValueStorage &storage, Tuple &fn_out) const
  {
    BLI_assert(storage.is_output_initialized(output_id));
    auto possible_target_ids = m_graph->targets_of_output(output_id);

    SocketInfo &output_info = m_output_info[output_id];
    CPPTypeInfo *type_info = output_info.type;
    void *value_src = storage.output_value_ptr(output_id);

    uint *target_ids = BLI_array_alloca(target_ids, possible_target_ids.size());
    uint target_amount = 0;
    for (uint possible_target_id : possible_target_ids) {
      if (!storage.is_input_initialized(possible_target_id)) {
        target_ids[target_amount] = possible_target_id;
        target_amount++;
      }
    }

    if (target_amount == 0) {
      type_info->destruct_type(value_src);
      storage.set_output_initialized(output_id, false);
    }
    else if (target_amount == 1) {
      uint target_id = target_ids[0];
      void *value_dst = storage.input_value_ptr(target_id);
      type_info->relocate_to_uninitialized(value_src, value_dst);
      storage.set_output_initialized(output_id, false);
      storage.set_input_initialized(target_id, true);
    }
    else {
      for (uint i = 1; i < target_amount; i++) {
        uint target_id = target_ids[i];
        void *value_dst = storage.input_value_ptr(target_id);
        type_info->copy_to_uninitialized(value_src, value_dst);
        storage.set_input_initialized(target_id, true);
      }

      uint target_id = target_ids[0];
      void *value_dst = storage.input_value_ptr(target_id);
      type_info->copy_to_uninitialized(value_src, value_dst);
      storage.set_output_initialized(output_id, false);
      storage.set_input_initialized(target_id, true);
    }

    for (uint i = 0; i < target_amount; i++) {
      uint target_id = target_ids[i];
      SocketInfo &socket_info = m_input_info[target_id];
      BLI_assert(type_info == socket_info.type);
      if (socket_info.is_fn_output) {
        uint index = m_fgraph.outputs().index(DFGraphSocket::FromInput(target_id));
        void *value_ptr = storage.input_value_ptr(target_id);
        fn_out.copy_in__dynamic(index, value_ptr);
      }
    }
  }

  void destruct_remaining_values(SocketValueStorage &storage) const
  {
    for (uint input_id = 0; input_id < m_inputs_init_buffer_size; input_id++) {
      if (storage.is_input_initialized(input_id)) {
        SocketInfo &socket_info = m_input_info[input_id];
        socket_info.type->destruct_type(storage.input_value_ptr(input_id));
      }
    }
    for (uint output_id = 0; output_id < m_outputs_init_buffer_size; output_id++) {
      if (storage.is_output_initialized(output_id)) {
        SocketInfo &socket_info = m_output_info[output_id];
        socket_info.type->destruct_type(storage.output_value_ptr(output_id));
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
