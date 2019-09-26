#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_vector_adaptor.h"

namespace FN {

using BLI::VectorAdaptor;

static void try_ensure_tuple_call_bodies(DataGraph &graph)
{
  for (uint node_id : graph.node_ids()) {
    SharedFunction &fn = graph.function_of_node(node_id);
    if (fn->has_body<TupleCallBody>()) {
      continue;
    }

    if (fn->has_body<LazyInTupleCallBody>()) {
      derive_TupleCallBody_from_LazyInTupleCallBody(fn);
    }

    if (fn->has_body<LLVMBuildIRBody>()) {
      derive_TupleCallBody_from_LLVMBuildIRBody(fn);
    }
  }
}

class ExecuteFGraph : public TupleCallBody {
 private:
  FunctionGraph m_fgraph;
  DataGraph &m_graph;

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

  Vector<NodeInfo> m_node_info;

  struct SocketInfo {
    CPPTypeInfo *type;
    uint offset;
    uint is_fn_output : 1;

    SocketInfo(CPPTypeInfo &type, uint offset, bool is_fn_output)
        : type(&type), offset(offset), is_fn_output(is_fn_output)
    {
    }
  };

  Vector<SocketInfo> m_input_info;
  Vector<SocketInfo> m_output_info;

  uint m_inputs_buffer_size = 0;
  uint m_outputs_buffer_size = 0;
  uint m_inputs_init_buffer_size = 0;
  uint m_outputs_init_buffer_size = 0;

 public:
  ExecuteFGraph(FunctionGraph &fgraph) : m_fgraph(fgraph), m_graph(fgraph.graph())
  {
    for (uint node_id : m_graph.node_ids()) {
      SharedFunction &fn = m_graph.function_of_node(node_id);

      TupleCallBodyBase *body = nullptr;
      bool is_lazy_body = false;
      if (fn->has_body<LazyInTupleCallBody>()) {
        body = &fn->body<LazyInTupleCallBody>();
        is_lazy_body = true;
      }
      else if (fn->has_body<TupleCallBody>()) {
        body = &fn->body<TupleCallBody>();
        is_lazy_body = false;
      }

      m_node_info.append(
          NodeInfo(body, is_lazy_body, m_inputs_buffer_size, m_outputs_buffer_size));

      m_inputs_init_buffer_size += fn->input_amount();
      m_outputs_init_buffer_size += fn->output_amount();

      if (body == nullptr) {
        for (auto type : fn->input_types()) {
          CPPTypeInfo &type_info = type->extension<CPPTypeInfo>();
          uint type_size = type_info.size();
          m_input_info.append(SocketInfo(type_info, m_inputs_buffer_size, false));
          m_inputs_buffer_size += type_size;
        }

        for (auto type : fn->output_types()) {
          CPPTypeInfo &type_info = type->extension<CPPTypeInfo>();
          uint type_size = type_info.size();
          m_output_info.append(SocketInfo(type_info, m_outputs_buffer_size, false));
          m_outputs_buffer_size += type_size;
        }
      }
      else {
        TupleMeta &meta_in = body->meta_in();
        for (uint i = 0; i < fn->input_amount(); i++) {
          m_input_info.append(SocketInfo(
              *meta_in.type_infos()[i], m_inputs_buffer_size + meta_in.offsets()[i], false));
        }
        m_inputs_buffer_size += meta_in.size_of_data();

        TupleMeta &meta_out = body->meta_out();
        for (uint i = 0; i < fn->output_amount(); i++) {
          m_output_info.append(SocketInfo(
              *meta_out.type_infos()[i], m_outputs_buffer_size + meta_out.offsets()[i], false));
        }
        m_outputs_buffer_size += meta_out.size_of_data();
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
                                    m_parent.m_graph.first_input_id_of_node(node_id));
    }

    bool *node_output_inits_ptr(uint node_id)
    {
      return (bool *)POINTER_OFFSET(m_output_inits,
                                    m_parent.m_graph.first_output_id_of_node(node_id));
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
    BLI_assert(fn_in.all_initialized());

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
      DataSocket socket = m_fgraph.inputs()[i];
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

  struct LazyStateOfNode {
    uint node_id;
    LazyState state;

    LazyStateOfNode(uint node_id, LazyState state) : node_id(node_id), state(state)
    {
    }
  };

#define SETUP_SUB_TUPLES(node_id, body, body_in, body_out) \
  Tuple body_in(body->meta_in(), \
                storage.node_input_values_ptr(node_id), \
                storage.node_input_inits_ptr(node_id), \
                true, \
                false); \
  Tuple body_out(body->meta_out(), \
                 storage.node_output_values_ptr(node_id), \
                 storage.node_output_inits_ptr(node_id), \
                 true, \
                 false);

  using SocketsToComputeStack = Stack<DataSocket, 64>;
  using LazyStatesStack = Stack<LazyStateOfNode>;

  void evaluate_graph_to_compute_outputs(SocketValueStorage &storage,
                                         Tuple &fn_out,
                                         ExecutionContext &ctx) const
  {
    SocketsToComputeStack sockets_to_compute;
    LazyStatesStack lazy_states;

    for (auto socket : m_fgraph.outputs()) {
      sockets_to_compute.push(socket);
    }

    while (!sockets_to_compute.empty()) {
      DataSocket socket = sockets_to_compute.peek();

      if (socket.is_input()) {
        if (storage.is_input_initialized(socket.id())) {
          sockets_to_compute.pop();
        }
        else {
          DataSocket origin = m_graph.origin_of_input(socket);
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
          uint node_id = m_graph.node_id_of_output(socket.id());

          if (m_node_info[node_id].is_lazy) {
            LazyInTupleCallBody *body = (LazyInTupleCallBody *)m_node_info[node_id].body;

            if (lazy_states.empty() || lazy_states.peek().node_id != node_id) {
              bool required_inputs_computed = this->ensure_required_inputs(
                  body, node_id, storage, sockets_to_compute);

              if (required_inputs_computed) {
                void *user_data = alloca(body->user_data_size());
                LazyState state(user_data);
                state.start_next_entry();

                SETUP_SUB_TUPLES(node_id, body, body_in, body_out);

                SourceInfo *source_info = m_graph.source_info_of_node(node_id);
                body->call__setup_stack(body_in, body_out, ctx, state, source_info);

                if (state.is_done()) {
                  this->destruct_remaining_node_inputs(node_id, storage);
                  this->copy_outputs_to_final_output_if_necessary(node_id, storage, fn_out);
                  sockets_to_compute.pop();
                }
                else {
                  this->push_requested_inputs_to_stack(
                      state, node_id, storage, sockets_to_compute);
                  lazy_states.push(LazyStateOfNode(node_id, state));
                }
              }
            }
            else {
              LazyState &state = lazy_states.peek().state;
              state.start_next_entry();

              SETUP_SUB_TUPLES(node_id, body, body_in, body_out);

              SourceInfo *source_info = m_graph.source_info_of_node(node_id);
              body->call__setup_stack(body_in, body_out, ctx, state, source_info);

              if (state.is_done()) {
                this->destruct_remaining_node_inputs(node_id, storage);
                this->copy_outputs_to_final_output_if_necessary(node_id, storage, fn_out);
                sockets_to_compute.pop();
                lazy_states.pop();
              }
              else {
                this->push_requested_inputs_to_stack(state, node_id, storage, sockets_to_compute);
              }
            }
          }
          else {
            bool all_inputs_computed = this->ensure_all_inputs(
                node_id, storage, sockets_to_compute);

            if (all_inputs_computed) {
              BLI_assert(!m_node_info[node_id].is_lazy);
              TupleCallBody *body = (TupleCallBody *)m_node_info[node_id].body;
              BLI_assert(body);

              SETUP_SUB_TUPLES(node_id, body, body_in, body_out);

              SourceInfo *source_info = m_graph.source_info_of_node(node_id);
              body->call__setup_stack(body_in, body_out, ctx, source_info);
              BLI_assert(body_out.all_initialized());

              this->destruct_remaining_node_inputs(node_id, storage);
              this->copy_outputs_to_final_output_if_necessary(node_id, storage, fn_out);
              sockets_to_compute.pop();
            }
          }
        }
      }
    }
  }

  bool ensure_required_inputs(LazyInTupleCallBody *body,
                              uint node_id,
                              SocketValueStorage &storage,
                              SocketsToComputeStack &sockets_to_compute) const
  {
    bool required_inputs_computed = true;
    for (uint input_index : body->always_required()) {
      uint input_id = m_graph.id_of_node_input(node_id, input_index);
      if (!storage.is_input_initialized(input_id)) {
        sockets_to_compute.push(DataSocket::FromInput(input_id));
        required_inputs_computed = false;
      }
    }
    return required_inputs_computed;
  }

  void push_requested_inputs_to_stack(LazyState &state,
                                      uint node_id,
                                      SocketValueStorage &storage,
                                      SocketsToComputeStack &sockets_to_compute) const
  {
    for (uint requested_input_index : state.requested_inputs()) {
      uint input_id = m_graph.id_of_node_input(node_id, requested_input_index);
      if (!storage.is_input_initialized(input_id)) {
        sockets_to_compute.push(DataSocket::FromInput(input_id));
      }
    }
  }

  bool ensure_all_inputs(uint node_id,
                         SocketValueStorage &storage,
                         SocketsToComputeStack &sockets_to_compute) const
  {
    bool all_inputs_computed = true;
    for (uint input_id : m_graph.input_ids_of_node(node_id)) {
      if (!storage.is_input_initialized(input_id)) {
        sockets_to_compute.push(DataSocket::FromInput(input_id));
        all_inputs_computed = false;
      }
    }
    return all_inputs_computed;
  }

  void copy_outputs_to_final_output_if_necessary(uint node_id,
                                                 SocketValueStorage &storage,
                                                 Tuple &fn_out) const
  {
    for (uint output_id : m_graph.output_ids_of_node(node_id)) {
      if (m_output_info[output_id].is_fn_output) {
        uint index = m_fgraph.outputs().index(DataSocket::FromOutput(output_id));
        fn_out.copy_in__dynamic(index, storage.output_value_ptr(output_id));
      }
    }
  }

  void destruct_remaining_node_inputs(uint node_id, SocketValueStorage &storage) const
  {
    for (uint input_id : m_graph.input_ids_of_node(node_id)) {
      if (storage.is_input_initialized(input_id)) {
        CPPTypeInfo *type_info = m_input_info[input_id].type;
        type_info->destruct(storage.input_value_ptr(input_id));
        storage.set_input_initialized(input_id, false);
      }
    }
  }

  void forward_output(uint output_id, SocketValueStorage &storage, Tuple &fn_out) const
  {
    BLI_assert(storage.is_output_initialized(output_id));
    auto possible_target_ids = m_graph.targets_of_output(output_id);

    const SocketInfo &output_info = m_output_info[output_id];
    CPPTypeInfo *type_info = output_info.type;

    uint *target_ids_array = (uint *)alloca(possible_target_ids.size() * sizeof(uint));
    VectorAdaptor<uint> target_ids(target_ids_array, possible_target_ids.size());

    this->filter_uninitialized_targets(possible_target_ids, storage, target_ids);

    this->forward_output_to_targets(output_id, target_ids, type_info, storage);
    this->copy_targets_to_final_output_if_necessary(target_ids, storage, fn_out);
  }

  void filter_uninitialized_targets(ArrayRef<uint> possible_target_ids,
                                    SocketValueStorage &storage,
                                    VectorAdaptor<uint> &r_target_ids) const
  {
    for (uint possible_target_id : possible_target_ids) {
      if (!storage.is_input_initialized(possible_target_id)) {
        r_target_ids.append(possible_target_id);
      }
    }
  }

  void forward_output_to_targets(uint output_id,
                                 ArrayRef<uint> target_ids,
                                 CPPTypeInfo *type_info,
                                 SocketValueStorage &storage) const
  {
    if (target_ids.size() == 0) {
      this->destruct_output(output_id, type_info, storage);
    }
    else if (target_ids.size() == 1) {
      this->relocate_output_to_input(output_id, target_ids[0], type_info, storage);
    }
    else {
      this->forward_output_to_multiple_inputs(output_id, target_ids, type_info, storage);
    }
  }

  void destruct_output(uint output_id, CPPTypeInfo *type_info, SocketValueStorage &storage) const
  {
    void *value_ptr = storage.output_value_ptr(output_id);
    type_info->destruct(value_ptr);
    storage.set_output_initialized(output_id, false);
  }

  void relocate_output_to_input(uint output_id,
                                uint target_id,
                                CPPTypeInfo *type_info,
                                SocketValueStorage &storage) const
  {
    void *value_src = storage.output_value_ptr(output_id);
    void *value_dst = storage.input_value_ptr(target_id);
    type_info->relocate_to_uninitialized(value_src, value_dst);
    storage.set_output_initialized(output_id, false);
    storage.set_input_initialized(target_id, true);
  }

  void forward_output_to_multiple_inputs(uint output_id,
                                         ArrayRef<uint> target_ids,
                                         CPPTypeInfo *type_info,
                                         SocketValueStorage &storage) const
  {
    void *value_src = storage.output_value_ptr(output_id);

    for (uint target_id : target_ids.drop_front()) {
      void *value_dst = storage.input_value_ptr(target_id);
      type_info->copy_to_uninitialized(value_src, value_dst);
      storage.set_input_initialized(target_id, true);
    }

    uint target_id = target_ids.first();
    void *value_dst = storage.input_value_ptr(target_id);
    type_info->relocate_to_uninitialized(value_src, value_dst);
    storage.set_output_initialized(output_id, false);
    storage.set_input_initialized(target_id, true);
  }

  void copy_targets_to_final_output_if_necessary(ArrayRef<uint> target_ids,
                                                 SocketValueStorage &storage,
                                                 Tuple &fn_out) const
  {
    for (uint target_id : target_ids) {
      const SocketInfo &socket_info = m_input_info[target_id];
      if (socket_info.is_fn_output) {
        uint index = m_fgraph.outputs().index(DataSocket::FromInput(target_id));
        void *value_ptr = storage.input_value_ptr(target_id);
        fn_out.copy_in__dynamic(index, value_ptr);
      }
    }
  }

  void destruct_remaining_values(SocketValueStorage &storage) const
  {
    for (uint input_id = 0; input_id < m_inputs_init_buffer_size; input_id++) {
      if (storage.is_input_initialized(input_id)) {
        const SocketInfo &socket_info = m_input_info[input_id];
        void *value_ptr = storage.input_value_ptr(input_id);
        socket_info.type->destruct(value_ptr);
      }
    }
    for (uint output_id = 0; output_id < m_outputs_init_buffer_size; output_id++) {
      if (storage.is_output_initialized(output_id)) {
        const SocketInfo &socket_info = m_output_info[output_id];
        void *value_ptr = storage.output_value_ptr(output_id);
        socket_info.type->destruct(value_ptr);
      }
    }
  }
};

class ExecuteFGraph_Simple : public TupleCallBody {
 private:
  FunctionGraph m_fgraph;
  /* Just for easy access. */
  DataGraph &m_graph;

 public:
  ExecuteFGraph_Simple(FunctionGraph &function_graph)
      : m_fgraph(function_graph), m_graph(function_graph.graph())
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DataSocket socket = m_fgraph.outputs()[i];
      this->compute_socket(fn_in, fn_out, i, socket, ctx);
    }
  }

  void compute_socket(
      Tuple &fn_in, Tuple &out, uint out_index, DataSocket socket, ExecutionContext &ctx) const
  {
    if (m_fgraph.inputs().contains(socket)) {
      uint index = m_fgraph.inputs().index(socket);
      Tuple::copy_element(fn_in, index, out, out_index);
    }
    else if (socket.is_input()) {
      this->compute_socket(fn_in, out, out_index, m_graph.origin_of_input(socket), ctx);
    }
    else {
      uint node_id = m_graph.node_id_of_output(socket);
      SharedFunction &fn = m_graph.function_of_node(node_id);
      TupleCallBody &body = fn->body<TupleCallBody>();

      FN_TUPLE_CALL_ALLOC_TUPLES(body, tmp_in, tmp_out);

      uint index = 0;
      for (DataSocket input_socket : m_graph.inputs_of_node(node_id)) {
        this->compute_socket(fn_in, tmp_in, index, input_socket, ctx);
        index++;
      }

      SourceInfoStackFrame node_frame(m_graph.source_info_of_node(node_id));
      body.call__setup_stack(tmp_in, tmp_out, ctx, node_frame);

      Tuple::copy_element(tmp_out, m_graph.index_of_output(socket.id()), out, out_index);
    }
  }
};

void fgraph_add_TupleCallBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  try_ensure_tuple_call_bodies(fgraph.graph());
  fn->add_body<ExecuteFGraph>(fgraph);
}

} /* namespace FN */
