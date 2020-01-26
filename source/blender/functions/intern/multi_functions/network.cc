#include "network.h"

#include "BLI_array_allocator.h"

namespace FN {

using BLI::ArrayAllocator;
using BLI::ScopedVector;

namespace OutputValueType {
enum Enum {
  SingleFromCaller,
  VectorFromCaller,
  Single,
  Vector,
};
}

struct OutputValue {
  OutputValueType::Enum type;
};

struct SingleFromCallerValue : public OutputValue {
  GenericVirtualListRef list_ref;
};

struct VectorFromCallerValue : public OutputValue {
  GenericVirtualListListRef list_list_ref;
};

struct SingleValue : public OutputValue {
  GenericMutableArrayRef array_ref;
  int max_remaining_users;
};

struct VectorValue : public OutputValue {
  GenericVectorArray *vector_array;
  int max_remaining_users;
};

class NetworkEvaluationStorage {
 private:
  MonotonicAllocator<256> m_monotonic_allocator;
  ArrayAllocator &m_array_allocator;
  IndexMask m_mask;
  Array<OutputValue *> m_value_per_output_id;

 public:
  NetworkEvaluationStorage(ArrayAllocator &array_allocator, IndexMask mask, uint socket_id_amount)
      : m_array_allocator(array_allocator),
        m_mask(mask),
        m_value_per_output_id(socket_id_amount, nullptr)
  {
    BLI_assert(array_allocator.array_size() >= mask.min_array_size());
  }

  ~NetworkEvaluationStorage()
  {
    for (OutputValue *any_value : m_value_per_output_id) {
      if (any_value == nullptr) {
        continue;
      }
      else if (any_value->type == OutputValueType::Single) {
        SingleValue *value = (SingleValue *)any_value;
        const CPPType &type = value->array_ref.type();
        type.destruct_indices(value->array_ref.buffer(), m_mask);
        m_array_allocator.deallocate(type.size(), value->array_ref.buffer());
      }
      else if (any_value->type == OutputValueType::Vector) {
        VectorValue *value = (VectorValue *)any_value;
        delete value->vector_array;
      }
    }
  }

  IndexMask mask() const
  {
    return m_mask;
  }

  void add_single_from_caller(const MFOutputSocket &socket, GenericVirtualListRef list_ref)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);

    auto *value = m_monotonic_allocator.allocate<SingleFromCallerValue>();
    m_value_per_output_id[socket.id()] = value;
    value->type = OutputValueType::SingleFromCaller;
    value->list_ref = list_ref;
  }

  void add_vector_from_caller(const MFOutputSocket &socket,
                              GenericVirtualListListRef list_list_ref)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);

    auto *value = m_monotonic_allocator.allocate<VectorFromCallerValue>();
    m_value_per_output_id[socket.id()] = value;
    value->type = OutputValueType::VectorFromCaller;
    value->list_list_ref = list_list_ref;
  }

  GenericMutableArrayRef allocate_single_output(const MFOutputSocket &socket)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);

    auto *value = m_monotonic_allocator.allocate<SingleValue>();
    m_value_per_output_id[socket.id()] = value;
    value->type = OutputValueType::Single;

    const CPPType &type = socket.data_type().single__cpp_type();
    void *buffer = m_array_allocator.allocate(type.size(), type.alignment());
    value->array_ref = GenericMutableArrayRef(type, buffer, m_mask.min_array_size());

    value->max_remaining_users = socket.targets().size();

    return value->array_ref;
  }

  GenericVectorArray &allocate_vector_output(const MFOutputSocket &socket)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);

    auto *value = m_monotonic_allocator.allocate<VectorValue>();
    m_value_per_output_id[socket.id()] = value;
    value->type = OutputValueType::Vector;

    const CPPType &type = socket.data_type().vector__cpp_base_type();
    GenericVectorArray *vector_array = new GenericVectorArray(type, m_mask.min_array_size());
    value->vector_array = vector_array;

    value->max_remaining_users = socket.targets().size();

    return *value->vector_array;
  }

  GenericMutableArrayRef get_mutable_single(const MFInputSocket &input,
                                            const MFOutputSocket &output)
  {
    const MFOutputSocket &from = input.origin();
    const MFOutputSocket &to = output;

    OutputValue *any_value = m_value_per_output_id[from.id()];
    BLI_assert(any_value != nullptr);
    BLI_assert(from.data_type().single__cpp_type() == to.data_type().single__cpp_type());

    if (any_value->type == OutputValueType::Single) {
      SingleValue *value = (SingleValue *)any_value;
      if (value->max_remaining_users == 1) {
        m_value_per_output_id[to.id()] = value;
        m_value_per_output_id[from.id()] = nullptr;
        value->max_remaining_users = to.targets().size();
        return value->array_ref;
      }
      else {
        SingleValue *new_value = m_monotonic_allocator.allocate<SingleValue>();
        m_value_per_output_id[to.id()] = new_value;
        new_value->type = OutputValueType::Single;
        new_value->max_remaining_users = to.targets().size();

        const CPPType &type = from.data_type().single__cpp_type();
        void *new_buffer = m_array_allocator.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized_indices(value->array_ref.buffer(), new_buffer, m_mask);
        new_value->array_ref = GenericMutableArrayRef(type, new_buffer, m_mask.min_array_size());
        return new_value->array_ref;
      }
    }
    else if (any_value->type == OutputValueType::SingleFromCaller) {
      SingleFromCallerValue *value = (SingleFromCallerValue *)any_value;
      SingleValue *new_value = m_monotonic_allocator.allocate<SingleValue>();
      m_value_per_output_id[to.id()] = new_value;
      new_value->type = OutputValueType::Single;
      new_value->max_remaining_users = to.targets().size();

      const CPPType &type = from.data_type().single__cpp_type();
      void *new_buffer = m_array_allocator.allocate(type.size(), type.alignment());
      new_value->array_ref = GenericMutableArrayRef(type, new_buffer, m_mask.min_array_size());
      value->list_ref.materialize_to_uninitialized(m_mask, new_value->array_ref);
      return new_value->array_ref;
    }

    BLI_assert(false);
    return GenericMutableArrayRef(CPP_TYPE<float>());
  }

  GenericVectorArray &get_mutable_vector(const MFInputSocket &input, const MFOutputSocket &output)
  {
    const MFOutputSocket &from = input.origin();
    const MFOutputSocket &to = output;

    OutputValue *any_value = m_value_per_output_id[from.id()];
    BLI_assert(any_value != nullptr);
    BLI_assert(from.data_type().vector__cpp_base_type() == to.data_type().vector__cpp_base_type());

    if (any_value->type == OutputValueType::Vector) {
      VectorValue *value = (VectorValue *)any_value;
      if (value->max_remaining_users == 1) {
        m_value_per_output_id[to.id()] = value;
        m_value_per_output_id[from.id()] = nullptr;
        value->max_remaining_users = to.targets().size();
        return *value->vector_array;
      }
      else {
        VectorValue *new_value = m_monotonic_allocator.allocate<VectorValue>();
        m_value_per_output_id[to.id()] = new_value;
        new_value->type = OutputValueType::Vector;
        new_value->max_remaining_users = to.targets().size();

        const CPPType &base_type = to.data_type().vector__cpp_base_type();
        new_value->vector_array = new GenericVectorArray(base_type, m_mask.min_array_size());
        new_value->vector_array->extend_multiple__copy(m_mask, *value->vector_array);

        return *new_value->vector_array;
      }
    }
    else if (any_value->type == OutputValueType::VectorFromCaller) {
      VectorFromCallerValue *value = (VectorFromCallerValue *)any_value;
      VectorValue *new_value = m_monotonic_allocator.allocate<VectorValue>();
      m_value_per_output_id[to.id()] = new_value;
      new_value->type = OutputValueType::Vector;
      new_value->max_remaining_users = to.targets().size();

      const CPPType &base_type = to.data_type().vector__cpp_base_type();
      new_value->vector_array = new GenericVectorArray(base_type, m_mask.min_array_size());

      for (uint i : m_mask) {
        new_value->vector_array->extend_single__copy(i, value->list_list_ref[i]);
      }

      return *new_value->vector_array;
    }

    BLI_assert(false);
    return *new GenericVectorArray(CPP_TYPE<float>(), 0);
  }

  void finish_input_socket(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();

    OutputValue *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    switch (any_value->type) {
      case OutputValueType::SingleFromCaller:
      case OutputValueType::VectorFromCaller: {
        break;
      }
      case OutputValueType::Single: {
        SingleValue *value = (SingleValue *)any_value;
        BLI_assert(value->max_remaining_users >= 1);
        value->max_remaining_users--;
        if (value->max_remaining_users == 0) {
          const CPPType &type = value->array_ref.type();
          type.destruct_indices(value->array_ref.buffer(), m_mask);
          m_array_allocator.deallocate(type.size(), value->array_ref.buffer());
          m_value_per_output_id[origin.id()] = nullptr;
        }
        break;
      }
      case OutputValueType::Vector: {
        VectorValue *value = (VectorValue *)any_value;
        BLI_assert(value->max_remaining_users >= 1);
        value->max_remaining_users--;
        if (value->max_remaining_users == 0) {
          delete value->vector_array;
          m_value_per_output_id[origin.id()] = nullptr;
        }
        break;
      }
    }
  }

  GenericVirtualListRef get_single_input(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();
    OutputValue *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    if (any_value->type == OutputValueType::Single) {
      SingleValue *value = (SingleValue *)any_value;
      return value->array_ref;
    }
    else if (any_value->type == OutputValueType::SingleFromCaller) {
      SingleFromCallerValue *value = (SingleFromCallerValue *)any_value;
      return value->list_ref;
    }

    BLI_assert(false);
    return GenericVirtualListRef(CPP_TYPE<float>());
  }

  GenericVirtualListListRef get_vector_input(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();
    OutputValue *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    if (any_value->type == OutputValueType::Vector) {
      VectorValue *value = (VectorValue *)any_value;
      return *value->vector_array;
    }
    else if (any_value->type == OutputValueType::VectorFromCaller) {
      VectorFromCallerValue *value = (VectorFromCallerValue *)any_value;
      return value->list_list_ref;
    }

    BLI_assert(false);
    return GenericVirtualListListRef::FromSingleArray(CPP_TYPE<float>(), nullptr, 0, 0);
  }

  bool socket_is_computed(const MFOutputSocket &socket)
  {
    OutputValue *any_value = m_value_per_output_id[socket.id()];
    return any_value != nullptr;
  }
};

MF_EvaluateNetwork::MF_EvaluateNetwork(Vector<const MFOutputSocket *> inputs,
                                       Vector<const MFInputSocket *> outputs)
    : m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
{
  BLI_assert(m_outputs.size() > 0);
  const MFNetwork &network = m_outputs[0]->node().network();

  MFSignatureBuilder signature = this->get_builder("Function Tree");

  Vector<const MFFunctionNode *> used_function_nodes = network.find_function_dependencies(
      m_outputs);
  for (const MFFunctionNode *node : used_function_nodes) {
    signature.copy_used_contexts(node->function());
  }

  for (auto socket : m_inputs) {
    BLI_assert(socket->node().is_dummy());

    MFDataType type = socket->data_type();
    switch (type.category()) {
      case MFDataType::Single:
        signature.single_input("Input", type.single__cpp_type());
        break;
      case MFDataType::Vector:
        signature.vector_input("Input", type.vector__cpp_base_type());
        break;
    }
  }

  for (auto socket : m_outputs) {
    BLI_assert(socket->node().is_dummy());

    MFDataType type = socket->data_type();
    switch (type.category()) {
      case MFDataType::Single:
        signature.single_output("Output", type.single__cpp_type());
        break;
      case MFDataType::Vector:
        signature.vector_output("Output", type.vector__cpp_base_type());
        break;
    }
  }
}

void MF_EvaluateNetwork::call(IndexMask mask, MFParams params, MFContext context) const
{
  if (mask.size() == 0) {
    return;
  }

  ArrayAllocator array_allocator(mask.min_array_size());
  const MFNetwork &network = m_outputs[0]->node().network();

  Storage storage(array_allocator, mask, network.socket_ids().size());
  this->copy_inputs_to_storage(params, storage);
  this->evaluate_network_to_compute_outputs(context, storage);
  this->copy_computed_values_to_outputs(params, storage);
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_inputs_to_storage(MFParams params,
                                                             Storage &storage) const
{
  for (uint input_index : m_inputs.index_range()) {
    const MFOutputSocket &socket = *m_inputs[input_index];
    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef input_list = params.readonly_single_input(input_index);
        storage.add_single_from_caller(socket, input_list);
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef input_list_list = params.readonly_vector_input(input_index);
        storage.add_vector_from_caller(socket, input_list_list);
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::evaluate_network_to_compute_outputs(
    MFContext &global_context, Storage &storage) const
{
  const MFNetwork &network = m_outputs[0]->node().network();
  ArrayRef<uint> max_dependency_depths = network.max_dependency_depth_per_node();

  Stack<const MFOutputSocket *> sockets_to_compute;
  for (const MFInputSocket *socket : m_outputs) {
    sockets_to_compute.push(&socket->origin());
  }

  while (!sockets_to_compute.is_empty()) {
    const MFOutputSocket &socket = *sockets_to_compute.peek();
    const MFNode &node = socket.node();

    if (node.is_dummy()) {
      BLI_assert(m_inputs.contains(&socket));
      continue;
    }

    const MFFunctionNode &function_node = node.as_function();

    ScopedVector<const MFOutputSocket *> missing_sockets;
    function_node.foreach_origin_socket([&](const MFOutputSocket &origin) {
      if (!storage.socket_is_computed(origin)) {
        missing_sockets.append(&origin);
      }
    });

    std::sort(missing_sockets.begin(),
              missing_sockets.end(),
              [&](const MFOutputSocket *a, const MFOutputSocket *b) {
                return max_dependency_depths[a->node().id()] <
                       max_dependency_depths[b->node().id()];
              });

    sockets_to_compute.push_multiple(missing_sockets.as_ref());

    bool all_inputs_are_computed = missing_sockets.size() == 0;
    if (all_inputs_are_computed) {
      this->evaluate_function(global_context, function_node, storage);
      sockets_to_compute.pop();
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::evaluate_function(MFContext &global_context,
                                                        const MFFunctionNode &function_node,
                                                        Storage &storage) const
{
  const MultiFunction &function = function_node.function();
  // std::cout << "Function: " << function.name() << "\n";

  MFParamsBuilder params_builder{function, storage.mask().min_array_size()};

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    switch (param_type.type()) {
      case MFParamType::SingleInput: {
        const MFInputSocket &socket = function_node.input_for_param(param_index);
        GenericVirtualListRef values = storage.get_single_input(socket);
        params_builder.add_readonly_single_input(values);
        break;
      }
      case MFParamType::VectorInput: {
        const MFInputSocket &socket = function_node.input_for_param(param_index);
        GenericVirtualListListRef values = storage.get_vector_input(socket);
        params_builder.add_readonly_vector_input(values);
        break;
      }
      case MFParamType::SingleOutput: {
        const MFOutputSocket &socket = function_node.output_for_param(param_index);
        GenericMutableArrayRef values = storage.allocate_single_output(socket);
        params_builder.add_single_output(values);
        break;
      }
      case MFParamType::VectorOutput: {
        const MFOutputSocket &socket = function_node.output_for_param(param_index);
        GenericVectorArray &values = storage.allocate_vector_output(socket);
        params_builder.add_vector_output(values);
        break;
      }
      case MFParamType::MutableSingle: {
        const MFInputSocket &input = function_node.input_for_param(param_index);
        const MFOutputSocket &output = function_node.output_for_param(param_index);
        GenericMutableArrayRef values = storage.get_mutable_single(input, output);
        params_builder.add_mutable_single(values);
        break;
      }
      case MFParamType::MutableVector: {
        const MFInputSocket &input = function_node.input_for_param(param_index);
        const MFOutputSocket &output = function_node.output_for_param(param_index);
        GenericVectorArray &values = storage.get_mutable_vector(input, output);
        params_builder.add_mutable_vector(values);
        break;
      }
    }
  }

  function.call(storage.mask(), params_builder, global_context);

  for (const MFInputSocket *socket : function_node.inputs()) {
    storage.finish_input_socket(*socket);
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_computed_values_to_outputs(MFParams params,
                                                                      Storage &storage) const
{
  for (uint output_index : m_outputs.index_range()) {
    uint global_param_index = m_inputs.size() + output_index;
    const MFInputSocket &socket = *m_outputs[output_index];

    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef values = storage.get_single_input(socket);
        GenericMutableArrayRef output_values = params.uninitialized_single_output(
            global_param_index);
        values.materialize_to_uninitialized(storage.mask(), output_values);
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef values = storage.get_vector_input(socket);
        GenericVectorArray &output_values = params.vector_output(global_param_index);
        output_values.extend_multiple__copy(storage.mask(), values);
        break;
      }
    }
  }
}

}  // namespace FN
