#include "network.h"

#include "BLI_array_allocator.h"

namespace FN {

using BLI::ArrayAllocator;

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

  GenericMutableArrayRef forward_mutable_single(const MFOutputSocket &from,
                                                const MFOutputSocket &to)
  {
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

  GenericVectorArray &forward_mutable_vector(const MFOutputSocket &from, const MFOutputSocket &to)
  {
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

        for (uint i : m_mask) {
          new_value->vector_array->extend_single__copy(i, (*value->vector_array)[i]);
        }

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
};

class MF_EvaluateNetwork_Storage {
 private:
  MonotonicAllocator<256> m_single_allocator;
  IndexMask m_mask;
  ArrayAllocator &m_array_allocator;
  Vector<GenericVectorArray *> m_vector_arrays;
  Vector<GenericMutableArrayRef> m_arrays;
  Vector<GenericMutableArrayRef> m_single_element_arrays;
  Map<uint, GenericVectorArray *> m_vector_array_for_inputs;
  Map<uint, GenericVirtualListRef> m_virtual_list_for_inputs;
  Map<uint, GenericVirtualListListRef> m_virtual_list_list_for_inputs;
  Map<uint, GenericMutableArrayRef> m_array_ref_for_inputs;

 public:
  MF_EvaluateNetwork_Storage(IndexMask mask, ArrayAllocator &array_allocator)
      : m_mask(mask), m_array_allocator(array_allocator)
  {
    BLI_assert(array_allocator.array_size() >= mask.min_array_size());
  }

  ~MF_EvaluateNetwork_Storage()
  {
    for (GenericVectorArray *vector_array : m_vector_arrays) {
      delete vector_array;
    }
    for (GenericMutableArrayRef array : m_arrays) {
      array.destruct_indices(m_mask);
      m_array_allocator.deallocate(array.type().size(), array.buffer());
    }
    for (GenericMutableArrayRef array : m_single_element_arrays) {
      array.destruct_indices(IndexMask(1));
    }
  }

  IndexMask &mask()
  {
    return m_mask;
  }

  GenericMutableArrayRef allocate_array(const CPPType &type)
  {
    uint size = m_mask.min_array_size();
    void *buffer = m_array_allocator.allocate(type.size(), type.alignment());
    GenericMutableArrayRef array(type, buffer, size);
    m_arrays.append(array);
    return array;
  }

  GenericMutableArrayRef allocate_array__single_element(const CPPType &type)
  {
    void *buffer = m_single_allocator.allocate(type.size(), type.alignment());
    GenericMutableArrayRef array(type, buffer, 1);
    m_single_element_arrays.append(array);
    return array;
  }

  GenericVectorArray &allocate_vector_array(const CPPType &type)
  {
    uint size = m_mask.min_array_size();
    GenericVectorArray *vector_array = new GenericVectorArray(type, size);
    m_vector_arrays.append(vector_array);
    return *vector_array;
  }

  GenericVectorArray &allocate_vector_array__single_element(const CPPType &type)
  {
    GenericVectorArray *vector_array = new GenericVectorArray(type, 1);
    m_vector_arrays.append(vector_array);
    return *vector_array;
  }

  GenericMutableArrayRef allocate_copy(GenericVirtualListRef array)
  {
    GenericMutableArrayRef new_array = this->allocate_array(array.type());
    array.materialize_to_uninitialized(m_mask, new_array);
    return new_array;
  }

  GenericVectorArray &allocate_copy(GenericVirtualListListRef vector_array)
  {
    GenericVectorArray &new_vector_array = this->allocate_vector_array(vector_array.type());
    for (uint i : m_mask.indices()) {
      new_vector_array.extend_single__copy(i, vector_array[i]);
    }
    return new_vector_array;
  }

  GenericMutableArrayRef allocate_single_copy(GenericMutableArrayRef array)
  {
    GenericMutableArrayRef new_array = this->allocate_array__single_element(array.type());
    new_array.copy_in__uninitialized(0, array[0]);
    return new_array;
  }

  GenericVectorArray &allocate_single_copy(GenericVectorArray &vector_array)
  {
    GenericVectorArray &new_vector_array = this->allocate_vector_array__single_element(
        vector_array.type());
    new_vector_array.extend_single__copy(0, vector_array[0]);
    return new_vector_array;
  }

  GenericMutableArrayRef allocate_full_copy_from_single(GenericMutableArrayRef array)
  {
    BLI_assert(array.size() == 1);
    GenericMutableArrayRef new_array = this->allocate_array(array.type());
    array.type().fill_uninitialized_indices(array[0], new_array.buffer(), m_mask);
    return new_array;
  }

  GenericVectorArray &allocate_full_copy_from_single(GenericVectorArray &vector_array)
  {
    BLI_assert(vector_array.size() == 1);
    GenericVectorArray &new_vector_array = this->allocate_vector_array(vector_array.type());
    for (uint i : m_mask.indices()) {
      new_vector_array.extend_single__copy(i, vector_array[0]);
    }
    return new_vector_array;
  }

  void set_array_ref(const MFInputSocket &socket, GenericMutableArrayRef array)
  {
    m_array_ref_for_inputs.add_new(socket.id(), array);
  }

  void set_virtual_list(const MFInputSocket &socket, GenericVirtualListRef list)
  {
    m_virtual_list_for_inputs.add_new(socket.id(), list);
  }

  void set_virtual_list_list(const MFInputSocket &socket, GenericVirtualListListRef list)
  {
    m_virtual_list_list_for_inputs.add_new(socket.id(), list);
  }

  void set_vector_array(const MFInputSocket &socket, GenericVectorArray &vector_array)
  {
    m_vector_array_for_inputs.add_new(socket.id(), &vector_array);
  }

  GenericVirtualListRef get_virtual_list(const MFInputSocket &socket) const
  {
    return m_virtual_list_for_inputs.lookup(socket.id());
  }

  GenericVirtualListListRef get_virtual_list_list(const MFInputSocket &socket) const
  {
    return m_virtual_list_list_for_inputs.lookup(socket.id());
  }

  GenericVectorArray &get_vector_array(const MFInputSocket &socket) const
  {
    return *m_vector_array_for_inputs.lookup(socket.id());
  }

  GenericMutableArrayRef get_array_ref(const MFInputSocket &socket) const
  {
    return m_array_ref_for_inputs.lookup(socket.id());
  }

  bool input_is_computed(const MFInputSocket &socket) const
  {
    switch (socket.data_type().category()) {
      case MFDataType::Single:
        return m_virtual_list_for_inputs.contains(socket.id());
      case MFDataType::Vector:
        return m_virtual_list_list_for_inputs.contains(socket.id()) ||
               m_vector_array_for_inputs.contains(socket.id());
    }
    BLI_assert(false);
    return false;
  }

  bool function_input_has_single_element(const MFInputSocket &socket) const
  {
    BLI_assert(socket.node().is_function());
    MFParamType param_type = socket.param_type();
    switch (param_type.type()) {
      case MFParamType::SingleInput:
        return m_virtual_list_for_inputs.lookup(socket.id()).is_single_element();
      case MFParamType::VectorInput:
        return m_virtual_list_list_for_inputs.lookup(socket.id()).is_single_list();
      case MFParamType::MutableSingle:
        return m_array_ref_for_inputs.lookup(socket.id()).size() == 1;
      case MFParamType::MutableVector:
        return m_vector_array_for_inputs.lookup(socket.id())->size() == 1;
      case MFParamType::SingleOutput:
      case MFParamType::VectorOutput:
        break;
    }
    BLI_assert(false);
    return false;
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

  Storage storage(mask, array_allocator);
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
        this->copy_inputs_to_storage__single(input_list, socket.targets(), storage);
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef input_list_list = params.readonly_vector_input(input_index);
        this->copy_inputs_to_storage__vector(input_list_list, socket.targets(), storage);
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_inputs_to_storage__single(
    GenericVirtualListRef input_list,
    ArrayRef<const MFInputSocket *> targets,
    Storage &storage) const
{
  for (const MFInputSocket *target : targets) {
    const MFNode &target_node = target->node();
    if (target_node.is_dummy()) {
      storage.set_virtual_list(*target, input_list);
    }
    else {
      MFParamType param_type = target->param_type();
      if (param_type.is_single_input()) {
        storage.set_virtual_list(*target, input_list);
      }
      else if (param_type.is_mutable_single()) {
        GenericMutableArrayRef array = storage.allocate_copy(input_list);
        storage.set_array_ref(*target, array);
      }
      else {
        BLI_assert(false);
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_inputs_to_storage__vector(
    GenericVirtualListListRef input_list_list,
    ArrayRef<const MFInputSocket *> targets,
    Storage &storage) const
{
  for (const MFInputSocket *target : targets) {
    const MFNode &target_node = target->node();
    if (target_node.is_dummy()) {
      storage.set_virtual_list_list(*target, input_list_list);
    }
    else {
      MFParamType param_type = target->param_type();
      if (param_type.is_vector_input()) {
        storage.set_virtual_list_list(*target, input_list_list);
      }
      else if (param_type.is_mutable_vector()) {
        GenericVectorArray &vector_array = storage.allocate_copy(input_list_list);
        storage.set_vector_array(*target, vector_array);
      }
      else {
        BLI_assert(false);
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::evaluate_network_to_compute_outputs(
    MFContext &global_context, Storage &storage) const
{
  Stack<const MFSocket *> sockets_to_compute;

  for (const MFInputSocket *input_socket : m_outputs) {
    sockets_to_compute.push(input_socket);
  }

  while (!sockets_to_compute.is_empty()) {
    const MFSocket &socket = *sockets_to_compute.peek();

    if (socket.is_input()) {
      const MFInputSocket &input_socket = socket.as_input();
      if (storage.input_is_computed(input_socket)) {
        sockets_to_compute.pop();
      }
      else {
        const MFOutputSocket &origin = input_socket.origin();
        sockets_to_compute.push(&origin);
      }
    }
    else {
      const MFOutputSocket &output_socket = socket.as_output();
      const MFFunctionNode &function_node = output_socket.node().as_function();

      uint not_computed_inputs_amount = 0;
      for (const MFInputSocket *input_socket : function_node.inputs()) {
        if (!storage.input_is_computed(*input_socket)) {
          not_computed_inputs_amount++;
          sockets_to_compute.push(input_socket);
        }
      }

      bool all_inputs_are_computed = not_computed_inputs_amount == 0;
      if (all_inputs_are_computed) {
        this->compute_and_forward_outputs(global_context, function_node, storage);
        sockets_to_compute.pop();
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::compute_and_forward_outputs(
    MFContext &global_context, const MFFunctionNode &function_node, Storage &storage) const
{
  const MultiFunction &function = function_node.function();

  if (this->can_evaluate_function_only_ones(function_node, storage)) {
    MFParamsBuilder params_builder(function, 1);

    this->prepare_function_params__single(function_node, storage, params_builder);
    function.call(IndexMask(1), params_builder, global_context);
    this->forward_computed_values__single(function_node, storage, params_builder);
  }
  else {
    MFParamsBuilder params_builder(function, storage.mask().min_array_size());

    this->prepare_function_params__all(function_node, storage, params_builder);
    function.call(storage.mask(), params_builder, global_context);
    this->forward_computed_values__all(function_node, storage, params_builder);
  }
}

BLI_NOINLINE bool MF_EvaluateNetwork::can_evaluate_function_only_ones(
    const MFFunctionNode &function_node, Storage &storage) const
{
  if (function_node.function().depends_on_per_element_context()) {
    return false;
  }

  for (const MFInputSocket *socket : function_node.inputs()) {
    if (!storage.function_input_has_single_element(*socket)) {
      return false;
    }
  }

  return true;
}

BLI_NOINLINE void MF_EvaluateNetwork::prepare_function_params__all(
    const MFFunctionNode &function_node, Storage &storage, MFParamsBuilder &params_builder) const
{
  const MultiFunction &function = function_node.function();
  uint array_size = storage.mask().min_array_size();

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    switch (param_type.type()) {
      case MFParamType::SingleInput: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVirtualListRef values = storage.get_virtual_list(input_socket);
        if (values.size() < array_size) {
          BLI_assert(values.is_single_element());
          values = GenericVirtualListRef::FromSingle(values.type(), values[0], array_size);
        }
        params_builder.add_readonly_single_input(values);
        break;
      }
      case MFParamType::VectorInput: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVirtualListListRef values = storage.get_virtual_list_list(input_socket);
        if (values.size() < array_size) {
          BLI_assert(values.is_single_list());
          values = values.extended_single_list(array_size);
        }
        params_builder.add_readonly_vector_input(values);
        break;
      }
      case MFParamType::SingleOutput: {
        GenericMutableArrayRef values_destination = storage.allocate_array(
            param_type.data_type().single__cpp_type());
        params_builder.add_single_output(values_destination);
        break;
      }
      case MFParamType::VectorOutput: {
        GenericVectorArray &values_destination = storage.allocate_vector_array(
            param_type.data_type().vector__cpp_base_type());
        params_builder.add_vector_output(values_destination);
        break;
      }
      case MFParamType::MutableSingle: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericMutableArrayRef values = storage.get_array_ref(input_socket);
        if (values.size() < array_size) {
          BLI_assert(values.size() == 1);
          GenericMutableArrayRef new_values = storage.allocate_full_copy_from_single(values);
          params_builder.add_mutable_single(new_values);
        }
        else {
          params_builder.add_mutable_single(values);
        }
        break;
      }
      case MFParamType::MutableVector: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVectorArray &values = storage.get_vector_array(input_socket);
        if (values.size() < array_size) {
          BLI_assert(values.size() == 1);
          GenericVectorArray &new_values = storage.allocate_full_copy_from_single(values);
          params_builder.add_mutable_vector(new_values);
        }
        else {
          params_builder.add_mutable_vector(values);
        }
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::forward_computed_values__all(
    const MFFunctionNode &function_node, Storage &storage, MFParamsBuilder &params_builder) const
{
  const MultiFunction &function = function_node.function();

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);

    switch (param_type.type()) {
      case MFParamType::SingleInput:
      case MFParamType::VectorInput:
        break;
      case MFParamType::SingleOutput:
      case MFParamType::MutableSingle: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericMutableArrayRef computed_values = params_builder.computed_array(param_index);
        for (const MFInputSocket *target : output_socket.targets()) {
          if (target->node().is_dummy()) {
            if (m_outputs.contains(target)) {
              storage.set_virtual_list(*target, computed_values);
            }
          }
          else {
            MFParamType target_param_type = target->param_type();
            if (target_param_type.is_single_input()) {
              storage.set_virtual_list(*target, computed_values);
            }
            else if (target_param_type.is_mutable_single()) {
              GenericMutableArrayRef copied_values = storage.allocate_copy(computed_values);
              storage.set_array_ref(*target, copied_values);
            }
            else {
              BLI_assert(false);
            }
          }
        }
        break;
      }
      case MFParamType::VectorOutput:
      case MFParamType::MutableVector: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericVectorArray &computed_values = params_builder.computed_vector_array(param_index);
        for (const MFInputSocket *target : output_socket.targets()) {
          if (target->node().is_dummy()) {
            if (m_outputs.contains(target)) {
              storage.set_virtual_list_list(*target, computed_values);
            }
          }
          else {
            MFParamType target_param_type = target->param_type();
            if (target_param_type.is_vector_input()) {
              storage.set_virtual_list_list(*target, computed_values);
            }
            else if (target_param_type.is_mutable_vector()) {
              GenericVectorArray &copied_values = storage.allocate_copy(computed_values);
              storage.set_vector_array(*target, copied_values);
            }
            else {
              BLI_assert(false);
            }
          }
        }
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::prepare_function_params__single(
    const MFFunctionNode &function_node, Storage &storage, MFParamsBuilder &params_builder) const
{
  const MultiFunction &function = function_node.function();

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    switch (param_type.type()) {
      case MFParamType::SingleInput: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVirtualListRef values = storage.get_virtual_list(input_socket);
        BLI_assert(values.is_single_element());
        params_builder.add_readonly_single_input(values);
        break;
      }
      case MFParamType::VectorInput: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVirtualListListRef values = storage.get_virtual_list_list(input_socket);
        BLI_assert(values.is_single_list());
        params_builder.add_readonly_vector_input(values);
        break;
      }
      case MFParamType::SingleOutput: {
        GenericMutableArrayRef values_destination = storage.allocate_array__single_element(
            param_type.data_type().single__cpp_type());
        params_builder.add_single_output(values_destination);
        break;
      }
      case MFParamType::VectorOutput: {
        GenericVectorArray &values_destination = storage.allocate_vector_array__single_element(
            param_type.data_type().vector__cpp_base_type());
        params_builder.add_vector_output(values_destination);
        break;
      }
      case MFParamType::MutableSingle: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericMutableArrayRef values = storage.get_array_ref(input_socket);
        params_builder.add_mutable_single(values);
        break;
      }
      case MFParamType::MutableVector: {
        const MFInputSocket &input_socket = function_node.input_for_param(param_index);
        GenericVectorArray &values = storage.get_vector_array(input_socket);
        params_builder.add_mutable_vector(values);
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::forward_computed_values__single(
    const MFFunctionNode &function_node, Storage &storage, MFParamsBuilder &params_builder) const
{
  const MultiFunction &function = function_node.function();

  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);

    switch (param_type.type()) {
      case MFParamType::SingleInput:
      case MFParamType::VectorInput:
        break;
      case MFParamType::SingleOutput:
      case MFParamType::MutableSingle: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericMutableArrayRef computed_value = params_builder.computed_array(param_index);
        for (const MFInputSocket *target : output_socket.targets()) {
          if (target->node().is_dummy()) {
            if (m_outputs.contains(target)) {
              storage.set_virtual_list(*target, computed_value);
            }
          }
          else {
            MFParamType target_param_type = target->param_type();
            if (target_param_type.is_single_input()) {
              storage.set_virtual_list(*target, computed_value);
            }
            else if (target_param_type.is_mutable_single()) {
              GenericMutableArrayRef copied_value = storage.allocate_single_copy(computed_value);
              storage.set_array_ref(*target, copied_value);
            }
            else {
              BLI_assert(false);
            }
          }
        }
        break;
      }
      case MFParamType::VectorOutput:
      case MFParamType::MutableVector: {
        const MFOutputSocket &output_socket = function_node.output_for_param(param_index);
        GenericVectorArray &computed_value = params_builder.computed_vector_array(param_index);
        for (const MFInputSocket *target : output_socket.targets()) {
          if (target->node().is_dummy()) {
            if (m_outputs.contains(target)) {
              storage.set_virtual_list_list(*target, computed_value);
            }
          }
          else {
            MFParamType target_param_type = target->param_type();
            if (target_param_type.is_vector_input()) {
              storage.set_virtual_list_list(*target, computed_value);
            }
            else if (target_param_type.is_mutable_vector()) {
              GenericVectorArray &copied_value = storage.allocate_single_copy(computed_value);
              storage.set_vector_array(*target, copied_value);
            }
          }
        }
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_computed_values_to_outputs(MFParams params,
                                                                      Storage &storage) const
{
  uint array_size = storage.mask().min_array_size();

  for (uint output_index : m_outputs.index_range()) {
    uint global_param_index = m_inputs.size() + output_index;
    const MFInputSocket &socket = *m_outputs[output_index];
    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef values = storage.get_virtual_list(socket);
        GenericMutableArrayRef output_values = params.uninitialized_single_output(
            global_param_index);
        if (values.size() < array_size) {
          BLI_assert(values.is_single_element());
          output_values.type().fill_uninitialized_indices(
              values[0], output_values.buffer(), storage.mask());
        }
        else {
          values.materialize_to_uninitialized(storage.mask(), output_values);
        }
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef values = storage.get_virtual_list_list(socket);
        GenericVectorArray &output_values = params.vector_output(global_param_index);
        if (values.size() < array_size) {
          BLI_assert(values.is_single_list());
          for (uint i : storage.mask().indices()) {
            output_values.extend_single__copy(i, values[0]);
          }
        }
        else {
          for (uint i : storage.mask().indices()) {
            output_values.extend_single__copy(i, values[i]);
          }
        }
        break;
      }
    }
  }
}

}  // namespace FN
