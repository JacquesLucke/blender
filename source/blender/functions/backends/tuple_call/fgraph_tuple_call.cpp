#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

class InterpretFGraph : public TupleCallBody {
 private:
  SharedDataFlowGraph m_graph;
  SocketSetVector m_inputs;
  SocketSetVector m_outputs;

  enum OpCode {
    InitTuple,
    DestructTuple,
    CopyElement,
    Call,
    GetInput,
    SetOutput,
    Forward,
  };

  struct Task {
    OpCode opcode;
    union {
      struct {
        TupleMeta *meta;
        uint index;
      } init_tuple;
      struct {
        uint index;
      } destruct_tuple;
      struct {
        struct {
          uint tuple, index;
        } from;
        struct {
          uint tuple, index;
        } to;
      } copy_element;
      struct {
        TupleCallBody *body;
        uint index_in, index_out;
      } call;
      struct {
        struct {
          uint index;
        } from;
        struct {
          uint tuple, index;
        } to;
      } get_input;
      struct {
        struct {
          uint tuple, index;
        } from;
        struct {
          uint index;
        } to;
      } set_output;
      struct {
        struct {
          uint index;
        } from;
        struct {
          uint index;
        } to;
      } forward;
    } data;
  };

  using TaskVector = SmallVector<Task>;

  uint m_combined_tuples_size;
  SmallVector<uint> m_tuple_offsets;
  NodeSetVector m_required_nodes;
  TaskVector m_tasks;

  struct NodeTupleIndices {
    uint in, out;
  };

  using TupleIndicesMap = SmallMap<Node *, NodeTupleIndices>;

 public:
  InterpretFGraph(FunctionGraph &fgraph)
      : m_graph(fgraph.graph()), m_inputs(fgraph.inputs()), m_outputs(fgraph.outputs())
  {
    SocketSet required_sockets = fgraph.find_used_sockets(false, true);

    NodeSetVector required_nodes;
    for (Socket socket : required_sockets) {
      if (socket.is_output()) {
        required_nodes.add(socket.node());
      }
    }

    auto *context = new llvm::LLVMContext();
    for (Node *node : required_nodes) {
      SharedFunction &fn = node->function();
      if (fn->has_body<TupleCallBody>()) {
        continue;
      }
      else if (fn->has_body<LLVMBuildIRBody>()) {
        derive_TupleCallBody_from_LLVMBuildIRBody(fn, *context);
      }
      else if (fn->has_body<LazyInTupleCallBody>()) {
        derive_TupleCallBody_from_LazyInTupleCallBody(fn);
      }
      else {
        BLI_assert(false);
      }
    }

    TupleIndicesMap tuple_indices;

    TaskVector destruct_tasks;

    m_combined_tuples_size = 0;
    m_tuple_offsets = {};
    for (Node *node : required_nodes) {
      TupleCallBody *body = node->function()->body<TupleCallBody>();
      BLI_assert(body != nullptr);

      m_tuple_offsets.append(m_combined_tuples_size);
      m_combined_tuples_size += body->meta_in()->size_of_full_tuple();
      m_tuple_offsets.append(m_combined_tuples_size);
      m_combined_tuples_size += body->meta_out()->size_of_full_tuple();

      NodeTupleIndices indices;
      indices.in = tuple_indices.size() * 2 + 0;
      indices.out = indices.in + 1;

      {
        Task init_in_task;
        init_in_task.opcode = OpCode::InitTuple;
        init_in_task.data.init_tuple.index = indices.in;
        init_in_task.data.init_tuple.meta = body->meta_in().ptr();
        m_tasks.append(init_in_task);

        Task init_out_tasks;
        init_out_tasks.opcode = OpCode::InitTuple;
        init_out_tasks.data.init_tuple.index = indices.out;
        init_out_tasks.data.init_tuple.meta = body->meta_out().ptr();
        m_tasks.append(init_out_tasks);

        Task destruct_in_task;
        destruct_in_task.opcode = OpCode::DestructTuple;
        destruct_in_task.data.destruct_tuple.index = indices.in;
        destruct_tasks.append(destruct_in_task);

        Task destruct_out_task;
        destruct_out_task.opcode = OpCode::DestructTuple;
        destruct_out_task.data.destruct_tuple.index = indices.out;
        destruct_tasks.append(destruct_out_task);
      }

      tuple_indices.add(node, indices);
    }

    NodeSet computed_nodes;

    for (uint i = 0; i < m_outputs.size(); i++) {
      this->compute_final_output(tuple_indices, computed_nodes, i);
    }

    m_tasks.extend(destruct_tasks);
  }

  void compute_final_output(TupleIndicesMap &tuple_indices, NodeSet &computed_nodes, uint index)
  {
    Socket socket = m_outputs[index];
    if (m_inputs.contains(socket)) {
      Task task;
      task.opcode = OpCode::Forward;
      task.data.forward.from.index = m_inputs.index(socket);
      task.data.forward.to.index = index;
      m_tasks.append(task);
    }
    else if (socket.is_input()) {
      Socket origin_socket = socket.origin();
      Node *origin_node = origin_socket.node();
      this->compute_node(tuple_indices, computed_nodes, origin_node);

      Task task;
      task.opcode = OpCode::SetOutput;
      task.data.set_output.from.tuple = tuple_indices.lookup_ref(origin_node).out;
      task.data.set_output.from.index = origin_socket.index();
      task.data.set_output.to.index = index;
      m_tasks.append(task);
    }
    else {
      this->compute_node(tuple_indices, computed_nodes, socket.node());

      Task task;
      task.opcode = OpCode::SetOutput;
      task.data.set_output.from.tuple = tuple_indices.lookup_ref(socket.node()).out;
      task.data.set_output.from.index = socket.index();
      task.data.set_output.to.index = index;
      m_tasks.append(task);
    }
  }

  void compute_node(TupleIndicesMap &tuple_indices, NodeSet &computed_nodes, Node *node)
  {
    if (computed_nodes.contains(node)) {
      return;
    }

    for (Socket input : node->inputs()) {
      Socket origin_socket = input.origin();

      if (m_inputs.contains(input)) {
        Task task;
        task.opcode = OpCode::GetInput;
        task.data.get_input.from.index = m_inputs.index(input);
        task.data.get_input.to.tuple = tuple_indices.lookup_ref(node).in;
        task.data.get_input.to.index = input.index();
        m_tasks.append(task);
      }
      else if (m_inputs.contains(origin_socket)) {
        Task task;
        task.opcode = OpCode::GetInput;
        task.data.get_input.from.index = m_inputs.index(origin_socket);
        task.data.get_input.to.tuple = tuple_indices.lookup_ref(node).in;
        task.data.get_input.to.index = input.index();
        m_tasks.append(task);
      }
      else {
        Node *origin_node = origin_socket.node();
        this->compute_node(tuple_indices, computed_nodes, origin_node);

        Task task;
        task.opcode = OpCode::CopyElement;
        task.data.copy_element.from.tuple = tuple_indices.lookup_ref(origin_node).out;
        task.data.copy_element.from.index = origin_socket.index();
        task.data.copy_element.to.tuple = tuple_indices.lookup_ref(node).in;
        task.data.copy_element.to.index = input.index();
        m_tasks.append(task);
      }
    }

    Task task;
    task.opcode = OpCode::Call;
    task.data.call.body = node->function()->body<TupleCallBody>();
    task.data.call.index_in = tuple_indices.lookup_ref(node).in;
    task.data.call.index_out = tuple_indices.lookup_ref(node).out;
    m_tasks.append(task);

    computed_nodes.add(node);
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    void *buffer = alloca(m_combined_tuples_size);
    for (Task &task : m_tasks) {
      switch (task.opcode) {
        case OpCode::InitTuple: {
          auto meta = SharedTupleMeta::FromPointer(task.data.init_tuple.meta);
          void *ptr = (char *)buffer + m_tuple_offsets[task.data.init_tuple.index];
          Tuple::ConstructInBuffer(meta, ptr);
          meta.extract_ptr();
          break;
        }
        case OpCode::DestructTuple: {
          Tuple &tuple = this->get_tuple(buffer, task.data.destruct_tuple.index);
          tuple.~Tuple();
          break;
        }
        case OpCode::CopyElement: {
          Tuple &from = this->get_tuple(buffer, task.data.copy_element.from.tuple);
          Tuple &to = this->get_tuple(buffer, task.data.copy_element.to.tuple);
          Tuple::copy_element(
              from, task.data.copy_element.from.index, to, task.data.copy_element.to.index);
          break;
        }
        case OpCode::Call: {
          Tuple &fn_in = this->get_tuple(buffer, task.data.call.index_in);
          Tuple &fn_out = this->get_tuple(buffer, task.data.call.index_out);
          task.data.call.body->call__setup_stack(fn_in, fn_out, ctx);
          break;
        }
        case OpCode::GetInput: {
          Tuple &to = this->get_tuple(buffer, task.data.get_input.to.tuple);
          Tuple::copy_element(
              fn_in, task.data.get_input.from.index, to, task.data.get_input.to.index);
          break;
        }
        case OpCode::SetOutput: {
          Tuple &from = this->get_tuple(buffer, task.data.set_output.from.tuple);
          Tuple::copy_element(
              from, task.data.set_output.from.index, fn_out, task.data.set_output.to.index);
          break;
        }
        case OpCode::Forward: {
          Tuple::copy_element(
              fn_in, task.data.forward.from.index, fn_out, task.data.forward.to.index);
          break;
        }
        default:
          BLI_assert(false);
          break;
      }
    }
  }

 private:
  Tuple &get_tuple(void *buffer, uint index) const
  {
    return *(Tuple *)((char *)buffer + m_tuple_offsets[index]);
  }
};

static SmallMap<Socket, uint> UNUSED_FUNCTION(get_max_usage_amounts)(SocketSet used_sockets)
{
  SmallMap<Socket, uint> usage_counts;

  for (Socket socket : used_sockets) {
    if (socket.is_input()) {
      continue;
    }
    uint amount = 0;
    for (Socket target : socket.targets()) {
      if (used_sockets.contains(target)) {
        amount++;
      }
    }
    usage_counts.add_new(socket, amount);
  }
  return usage_counts;
}

class ExecuteGraph : public TupleCallBody {
 private:
  SharedDataFlowGraph m_graph;
  SocketSetVector m_inputs;
  SocketSetVector m_outputs;

 public:
  ExecuteGraph(const FunctionGraph &function_graph)
      : m_graph(function_graph.graph()),
        m_inputs(function_graph.inputs()),
        m_outputs(function_graph.outputs())
  {
    auto *context = new llvm::LLVMContext();

    for (Node *node : m_graph->all_nodes()) {
      SharedFunction &fn = node->function();
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
    for (uint i = 0; i < m_outputs.size(); i++) {
      this->compute_socket(fn_in, fn_out, i, m_outputs[i], ctx);
    }
  }

  void compute_socket(
      Tuple &fn_in, Tuple &out, uint out_index, Socket socket, ExecutionContext &ctx) const
  {
    if (m_inputs.contains(socket)) {
      uint index = m_inputs.index(socket);
      Tuple::copy_element(fn_in, index, out, out_index);
    }
    else if (socket.is_input()) {
      this->compute_socket(fn_in, out, out_index, socket.origin(), ctx);
    }
    else {
      Node *node = socket.node();
      SharedFunction &fn = node->function();
      TupleCallBody *body = fn->body<TupleCallBody>();

      FN_TUPLE_CALL_ALLOC_TUPLES(body, tmp_in, tmp_out);

      for (uint i = 0; i < node->input_amount(); i++) {
        this->compute_socket(fn_in, tmp_in, i, node->input(i), ctx);
      }

      SourceInfoStackFrame node_frame(node->source());
      body->call__setup_stack(tmp_in, tmp_out, ctx, node_frame);

      Tuple::copy_element(tmp_out, socket.index(), out, out_index);
    }
  }
};

class LazyExecFGraph : public TupleCallBody {
 private:
  FunctionGraph m_fgraph;

 public:
  LazyExecFGraph(FunctionGraph &fgraph) : m_fgraph(fgraph)
  {
    auto *context = new llvm::LLVMContext();

    for (Node *node : fgraph.graph()->all_nodes()) {
      if (node->function()->has_body<TupleCallBody>()) {
        continue;
      }
      if (node->function()->has_body<LazyInTupleCallBody>()) {
        continue;
      }
      if (node->function()->has_body<LLVMBuildIRBody>()) {
        derive_TupleCallBody_from_LLVMBuildIRBody(node->function(), *context);
      }
    }
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    SocketSet required_sockets = m_fgraph.find_used_sockets(false, true);
    for (Socket socket : m_fgraph.inputs()) {
      required_sockets.add(socket);
    }

    SmallVector<SharedType> temp_storage_types;
    SmallMap<Socket, uint> socket_indices;

    uint index = 0;
    for (Socket socket : required_sockets) {
      temp_storage_types.append(socket.type());
      socket_indices.add(socket, index);
      index++;
    }

    auto temp_meta = SharedTupleMeta::New(temp_storage_types);
    Tuple temp_storage(temp_meta);

    for (uint i = 0; i < m_fgraph.inputs().size(); i++) {
      Socket socket = m_fgraph.inputs()[i];
      Tuple::relocate_element(fn_in, i, temp_storage, socket_indices.lookup(socket));
    }

    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      Socket socket = m_fgraph.outputs()[i];
      this->compute_socket(socket, temp_storage, socket_indices, ctx);
    }

    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      Socket socket = m_fgraph.outputs()[i];
      Tuple::relocate_element(temp_storage, socket_indices.lookup(socket), fn_out, i);
    }
  }

  void compute_socket(Socket socket,
                      Tuple &temp_storage,
                      const SmallMap<Socket, uint> &socket_indices,
                      ExecutionContext &ctx) const
  {
    uint socket_index = socket_indices.lookup(socket);
    if (temp_storage.is_initialized(socket_index)) {
      /* do nothing */
    }
    else if (socket.is_input()) {
      Socket origin_socket = socket.origin();
      this->compute_socket(origin_socket, temp_storage, socket_indices, ctx);
      Tuple::copy_element(temp_storage,
                          socket_indices.lookup(origin_socket),
                          temp_storage,
                          socket_indices.lookup(socket));
    }
    else if (socket.is_output()) {
      Node *node = socket.node();
      SharedFunction &fn = node->function();

      if (fn->has_body<LazyInTupleCallBody>()) {
        auto *body = node->function()->body<LazyInTupleCallBody>();

        FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

        for (uint input_index : body->always_required()) {
          Socket input_socket = node->input(input_index);
          this->compute_socket(input_socket, temp_storage, socket_indices, ctx);
          Tuple::copy_element(
              temp_storage, socket_indices.lookup(input_socket), fn_in, input_index);
        }

        void *user_data = alloca(body->user_data_size());
        LazyState lazy_state(user_data);

        while (!lazy_state.is_done()) {
          lazy_state.start_next_entry();
          body->call(fn_in, fn_out, ctx, lazy_state);
          for (uint input_index : lazy_state.requested_inputs()) {
            Socket input_socket = node->input(input_index);
            this->compute_socket(input_socket, temp_storage, socket_indices, ctx);
            Tuple::copy_element(
                temp_storage, socket_indices.lookup(input_socket), fn_in, input_index);
          }
        }
        BLI_assert(fn_out.all_initialized());

        for (uint output_index = 0; output_index < node->output_amount(); output_index++) {
          Socket output_socket = node->output(output_index);
          Tuple::relocate_element(
              fn_out, output_index, temp_storage, socket_indices.lookup(output_socket));
        }
      }
      else if (fn->has_body<TupleCallBody>()) {
        auto *body = node->function()->body<TupleCallBody>();

        FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

        for (uint input_index = 0; input_index < node->input_amount(); input_index++) {
          Socket input_socket = node->input(input_index);
          this->compute_socket(input_socket, temp_storage, socket_indices, ctx);
          Tuple::copy_element(
              temp_storage, socket_indices.lookup(input_socket), fn_in, input_index);
        }

        body->call__setup_stack(fn_in, fn_out, ctx);
        BLI_assert(fn_out.all_initialized());

        for (uint output_index = 0; output_index < node->output_amount(); output_index++) {
          Socket output_socket = node->output(output_index);
          uint temp_index = socket_indices.lookup_default(output_socket, -1);
          if (temp_index != -1) {
            Tuple::relocate_element(fn_out, output_index, temp_storage, temp_index);
          }
        }
      }
      else {
        BLI_assert(false);
      }
    }
  }
};

void fgraph_add_TupleCallBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  fn->add_body(new ExecuteGraph(fgraph));
  // fn->add_body(new InterpretFGraph(fgraph));
  // fn->add_body(new LazyExecFGraph(fgraph));
}

} /* namespace FN */
