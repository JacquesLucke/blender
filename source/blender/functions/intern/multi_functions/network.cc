#include "network.h"

#include "BLI_buffer_cache.h"

namespace FN {

using BLI::BufferCache;
using BLI::ScopedVector;

namespace {

namespace ValueType {
enum Enum {
  InputSingle,
  InputVector,
  OutputSingle,
  OutputVector,
  OwnSingle,
  OwnVector,
};
}

struct Value {
  ValueType::Enum type;

  Value(ValueType::Enum type) : type(type)
  {
  }
};

struct InputSingleValue : public Value {
  GenericVirtualListRef list_ref;

  InputSingleValue(GenericVirtualListRef list_ref)
      : Value(ValueType::InputSingle), list_ref(list_ref)
  {
  }
};

struct InputVectorValue : public Value {
  GenericVirtualListListRef list_list_ref;

  InputVectorValue(GenericVirtualListListRef list_list_ref)
      : Value(ValueType::InputVector), list_list_ref(list_list_ref)
  {
  }
};

struct OutputValue : public Value {
  bool is_computed = false;

  OutputValue(ValueType::Enum type) : Value(type)
  {
  }
};

struct OutputSingleValue : public OutputValue {
  GenericMutableArrayRef array_ref;

  OutputSingleValue(GenericMutableArrayRef array_ref)
      : OutputValue(ValueType::OutputSingle), array_ref(array_ref)
  {
  }
};

struct OutputVectorValue : public OutputValue {
  GenericVectorArray *vector_array;

  OutputVectorValue(GenericVectorArray &vector_array)
      : OutputValue(ValueType::OutputVector), vector_array(&vector_array)
  {
  }
};

struct OwnSingleValue : public Value {
  GenericMutableArrayRef array_ref;
  int max_remaining_users;
  bool is_single_allocated;

  OwnSingleValue(GenericMutableArrayRef array_ref,
                 int max_remaining_users,
                 bool is_single_allocated)
      : Value(ValueType::OwnSingle),
        array_ref(array_ref),
        max_remaining_users(max_remaining_users),
        is_single_allocated(is_single_allocated)
  {
  }
};

struct OwnVectorValue : public Value {
  GenericVectorArray *vector_array;
  int max_remaining_users;

  OwnVectorValue(GenericVectorArray &vector_array, int max_remaining_users)
      : Value(ValueType::OwnVector),
        vector_array(&vector_array),
        max_remaining_users(max_remaining_users)
  {
  }
};

}  // namespace

class NetworkEvaluationStorage {
 private:
  LinearAllocator<> m_allocator;
  BufferCache &m_buffer_cache;
  IndexMask m_mask;
  Array<Value *> m_value_per_output_id;
  uint m_min_array_size;

 public:
  NetworkEvaluationStorage(BufferCache &buffer_cache, IndexMask mask, uint socket_id_amount)
      : m_buffer_cache(buffer_cache),
        m_mask(mask),
        m_value_per_output_id(socket_id_amount, nullptr),
        m_min_array_size(mask.min_array_size())
  {
  }

  ~NetworkEvaluationStorage()
  {
    for (Value *any_value : m_value_per_output_id) {
      if (any_value == nullptr) {
        continue;
      }
      else if (any_value->type == ValueType::OwnSingle) {
        OwnSingleValue *value = (OwnSingleValue *)any_value;
        GenericMutableArrayRef array_ref = value->array_ref;
        const CPPType &type = array_ref.type();
        if (value->is_single_allocated) {
          type.destruct(array_ref.buffer());
        }
        else {
          type.destruct_indices(array_ref.buffer(), m_mask);
          m_buffer_cache.deallocate(array_ref.buffer());
        }
      }
      else if (any_value->type == ValueType::OwnVector) {
        OwnVectorValue *value = (OwnVectorValue *)any_value;
        delete value->vector_array;
      }
    }
  }

  IndexMask mask() const
  {
    return m_mask;
  }

  bool socket_is_computed(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      return false;
    }
    if (ELEM(any_value->type, ValueType::OutputSingle, ValueType::OutputVector)) {
      return ((OutputValue *)any_value)->is_computed;
    }
    return true;
  }

  bool is_same_value_for_every_index(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    switch (any_value->type) {
      case ValueType::OwnSingle:
        return ((OwnSingleValue *)any_value)->array_ref.size() == 1;
      case ValueType::OwnVector:
        return ((OwnVectorValue *)any_value)->vector_array->size() == 1;
      case ValueType::InputSingle:
        return ((InputSingleValue *)any_value)->list_ref.is_single_element();
      case ValueType::InputVector:
        return ((InputVectorValue *)any_value)->list_list_ref.is_single_list();
      case ValueType::OutputSingle:
        return ((OutputSingleValue *)any_value)->array_ref.size() == 1;
      case ValueType::OutputVector:
        return ((OutputVectorValue *)any_value)->vector_array->size() == 1;
    }
    BLI_assert(false);
    return false;
  }

  bool socket_has_buffer_for_output(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      return false;
    }

    BLI_assert(ELEM(any_value->type, ValueType::OutputSingle, ValueType::OutputVector));
    return true;
  }

  void finish_output_socket(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      return;
    }

    if (ELEM(any_value->type, ValueType::OutputSingle, ValueType::OutputVector)) {
      ((OutputValue *)any_value)->is_computed = true;
    }
  }

  void finish_input_socket(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();

    Value *any_value = m_value_per_output_id[origin.id()];
    if (any_value == nullptr) {
      /* Can happen when a value has been forward to the next node. */
      return;
    }

    switch (any_value->type) {
      case ValueType::InputSingle:
      case ValueType::OutputSingle:
      case ValueType::InputVector:
      case ValueType::OutputVector: {
        break;
      }
      case ValueType::OwnSingle: {
        OwnSingleValue *value = (OwnSingleValue *)any_value;
        BLI_assert(value->max_remaining_users >= 1);
        value->max_remaining_users--;
        if (value->max_remaining_users == 0) {
          GenericMutableArrayRef array_ref = value->array_ref;
          const CPPType &type = array_ref.type();
          if (value->is_single_allocated) {
            type.destruct(array_ref.buffer());
          }
          else {
            type.destruct_indices(array_ref.buffer(), m_mask);
            m_buffer_cache.deallocate(array_ref.buffer());
          }
          m_value_per_output_id[origin.id()] = nullptr;
        }
        break;
      }
      case ValueType::OwnVector: {
        OwnVectorValue *value = (OwnVectorValue *)any_value;
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

  /* Add function inputs from caller to the storage.
   ********************************************************/

  void add_single_input_from_caller(const MFOutputSocket &socket, GenericVirtualListRef list_ref)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);
    BLI_assert(list_ref.size() >= m_min_array_size);

    auto *value = m_allocator.construct<InputSingleValue>(list_ref);
    m_value_per_output_id[socket.id()] = value;
  }

  void add_vector_input_from_caller(const MFOutputSocket &socket,
                                    GenericVirtualListListRef list_list_ref)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);
    BLI_assert(list_list_ref.size() >= m_min_array_size);

    auto *value = m_allocator.construct<InputVectorValue>(list_list_ref);
    m_value_per_output_id[socket.id()] = value;
  }

  /* Add function outputs from caller to the storage.
   *******************************************************/

  void add_single_output_from_caller(const MFOutputSocket &socket,
                                     GenericMutableArrayRef array_ref)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);
    BLI_assert(array_ref.size() >= m_min_array_size);

    auto *value = m_allocator.construct<OutputSingleValue>(array_ref);
    m_value_per_output_id[socket.id()] = value;
  }

  void add_vector_output_from_caller(const MFOutputSocket &socket,
                                     GenericVectorArray &vector_array)
  {
    BLI_assert(m_value_per_output_id[socket.id()] == nullptr);
    BLI_assert(vector_array.size() >= m_min_array_size);

    auto *value = m_allocator.construct<OutputVectorValue>(vector_array);
    m_value_per_output_id[socket.id()] = value;
  }

  /* Get memory for the output of individual function calls.
   ********************************************************************/

  GenericMutableArrayRef get_single_output__full(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      const CPPType &type = socket.data_type().single__cpp_type();
      void *buffer = m_buffer_cache.allocate(m_min_array_size, type.size(), type.alignment());
      GenericMutableArrayRef array_ref(type, buffer, m_min_array_size);

      auto *value = m_allocator.construct<OwnSingleValue>(
          array_ref, socket.target_amount(), false);
      m_value_per_output_id[socket.id()] = value;

      return array_ref;
    }
    else {
      BLI_assert(any_value->type == ValueType::OutputSingle);
      return ((OutputSingleValue *)any_value)->array_ref;
    }
  }

  GenericMutableArrayRef get_single_output__single(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      const CPPType &type = socket.data_type().single__cpp_type();
      void *buffer = m_allocator.allocate(type.size(), type.alignment());
      GenericMutableArrayRef array_ref(type, buffer, 1);

      auto *value = m_allocator.construct<OwnSingleValue>(array_ref, socket.target_amount(), true);
      m_value_per_output_id[socket.id()] = value;

      return value->array_ref;
    }
    else {
      BLI_assert(any_value->type == ValueType::OutputSingle);
      GenericMutableArrayRef array_ref = ((OutputSingleValue *)any_value)->array_ref;
      BLI_assert(array_ref.size() == 1);
      return array_ref;
    }
  }

  GenericVectorArray &get_vector_output__full(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      const CPPType &type = socket.data_type().vector__cpp_base_type();
      GenericVectorArray *vector_array = new GenericVectorArray(type, m_min_array_size);

      auto *value = m_allocator.construct<OwnVectorValue>(*vector_array, socket.target_amount());
      m_value_per_output_id[socket.id()] = value;

      return *value->vector_array;
    }
    else {
      BLI_assert(any_value->type == ValueType::OutputVector);
      return *((OutputVectorValue *)any_value)->vector_array;
    }
  }

  GenericVectorArray &get_vector_output__single(const MFOutputSocket &socket)
  {
    Value *any_value = m_value_per_output_id[socket.id()];
    if (any_value == nullptr) {
      const CPPType &type = socket.data_type().vector__cpp_base_type();
      GenericVectorArray *vector_array = new GenericVectorArray(type, 1);

      auto *value = m_allocator.construct<OwnVectorValue>(*vector_array, socket.target_amount());
      m_value_per_output_id[socket.id()] = value;

      return *value->vector_array;
    }
    else {
      BLI_assert(any_value->type == ValueType::OutputVector);
      GenericVectorArray &vector_array = *((OutputVectorValue *)any_value)->vector_array;
      BLI_assert(vector_array.size() == 1);
      return vector_array;
    }
  }

  /* Get a mutable memory for a function that wants to mutate date.
   **********************************************************************/

  GenericMutableArrayRef get_mutable_single__full(const MFInputSocket &input,
                                                  const MFOutputSocket &output)
  {
    const MFOutputSocket &from = input.origin();
    const MFOutputSocket &to = output;
    const CPPType &type = from.data_type().single__cpp_type();

    Value *from_any_value = m_value_per_output_id[from.id()];
    Value *to_any_value = m_value_per_output_id[to.id()];
    BLI_assert(from_any_value != nullptr);
    BLI_assert(type == to.data_type().single__cpp_type());

    if (to_any_value != nullptr) {
      BLI_assert(to_any_value->type == ValueType::OutputSingle);
      GenericMutableArrayRef array_ref = ((OutputSingleValue *)to_any_value)->array_ref;
      GenericVirtualListRef list_ref = this->get_single_input__full(input);
      list_ref.materialize_to_uninitialized(m_mask, array_ref);
      return array_ref;
    }

    if (from_any_value->type == ValueType::OwnSingle) {
      OwnSingleValue *value = (OwnSingleValue *)from_any_value;
      if (value->max_remaining_users == 1 && !value->is_single_allocated) {
        m_value_per_output_id[to.id()] = value;
        m_value_per_output_id[from.id()] = nullptr;
        value->max_remaining_users = to.target_amount();
        return value->array_ref;
      }
    }

    GenericVirtualListRef list_ref = this->get_single_input__full(input);
    void *new_buffer = m_buffer_cache.allocate(m_min_array_size, type.size(), type.alignment());
    GenericMutableArrayRef new_array_ref(type, new_buffer, m_min_array_size);
    list_ref.materialize_to_uninitialized(m_mask, new_array_ref);

    OwnSingleValue *new_value = m_allocator.construct<OwnSingleValue>(
        new_array_ref, to.target_amount(), false);
    m_value_per_output_id[to.id()] = new_value;
    return new_array_ref;
  }

  GenericMutableArrayRef get_mutable_single__single(const MFInputSocket &input,
                                                    const MFOutputSocket &output)
  {
    const MFOutputSocket &from = input.origin();
    const MFOutputSocket &to = output;
    const CPPType &type = from.data_type().single__cpp_type();

    Value *from_any_value = m_value_per_output_id[from.id()];
    Value *to_any_value = m_value_per_output_id[to.id()];
    BLI_assert(from_any_value != nullptr);
    BLI_assert(type == to.data_type().single__cpp_type());

    if (to_any_value != nullptr) {
      BLI_assert(to_any_value->type == ValueType::OutputSingle);
      GenericMutableArrayRef array_ref = ((OutputSingleValue *)to_any_value)->array_ref;
      BLI_assert(array_ref.size() == 1);
      GenericVirtualListRef list_ref = this->get_single_input__single(input);
      type.copy_to_uninitialized(list_ref.as_single_element(), array_ref[0]);
      return array_ref;
    }

    if (from_any_value->type == ValueType::OwnSingle) {
      OwnSingleValue *value = (OwnSingleValue *)from_any_value;
      if (value->max_remaining_users == 1) {
        m_value_per_output_id[to.id()] = value;
        m_value_per_output_id[from.id()] = nullptr;
        value->max_remaining_users = to.target_amount();
        BLI_assert(value->array_ref.size() == 1);
        return value->array_ref;
      }
    }

    GenericVirtualListRef list_ref = this->get_single_input__single(input);

    void *new_buffer = m_allocator.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(list_ref.as_single_element(), new_buffer);
    GenericMutableArrayRef new_array_ref(type, new_buffer, 1);

    OwnSingleValue *new_value = m_allocator.construct<OwnSingleValue>(
        new_array_ref, to.target_amount(), true);
    m_value_per_output_id[to.id()] = new_value;
    return new_array_ref;
  }

  GenericVectorArray &get_mutable_vector__full(const MFInputSocket &input,
                                               const MFOutputSocket &output)
  {
    const MFOutputSocket &from = input.origin();
    const MFOutputSocket &to = output;
    const CPPType &base_type = from.data_type().vector__cpp_base_type();

    Value *from_any_value = m_value_per_output_id[from.id()];
    Value *to_any_value = m_value_per_output_id[to.id()];
    BLI_assert(from_any_value != nullptr);
    BLI_assert(base_type == to.data_type().vector__cpp_base_type());

    if (to_any_value != nullptr) {
      BLI_assert(to_any_value->type == ValueType::OutputVector);
      GenericVectorArray &vector_array = *((OutputVectorValue *)to_any_value)->vector_array;
      GenericVirtualListListRef list_list_ref = this->get_vector_input__full(input);
      vector_array.extend_multiple__copy(m_mask, list_list_ref);
      return vector_array;
    }

    if (from_any_value->type == ValueType::OwnVector) {
      OwnVectorValue *value = (OwnVectorValue *)from_any_value;
      if (value->max_remaining_users == 1) {
        m_value_per_output_id[to.id()] = value;
        m_value_per_output_id[from.id()] = nullptr;
        value->max_remaining_users = to.target_amount();
        return *value->vector_array;
      }
    }

    GenericVirtualListListRef list_list_ref = this->get_vector_input__full(input);

    GenericVectorArray *new_vector_array = new GenericVectorArray(base_type, m_min_array_size);
    new_vector_array->extend_multiple__copy(m_mask, list_list_ref);

    OwnVectorValue *new_value = m_allocator.construct<OwnVectorValue>(*new_vector_array,
                                                                      to.target_amount());
    m_value_per_output_id[to.id()] = new_value;

    return *new_vector_array;
  }

  GenericVectorArray &get_mutable_vector__single(const MFInputSocket &input,
                                                 const MFOutputSocket &output)
  {
    const MFOutputSocket &from = input.origin();
    const MFOutputSocket &to = output;
    const CPPType &base_type = from.data_type().vector__cpp_base_type();

    Value *from_any_value = m_value_per_output_id[from.id()];
    Value *to_any_value = m_value_per_output_id[to.id()];
    BLI_assert(from_any_value != nullptr);
    BLI_assert(base_type == to.data_type().vector__cpp_base_type());

    if (to_any_value != nullptr) {
      BLI_assert(to_any_value->type == ValueType::OutputVector);
      GenericVectorArray &vector_array = *((OutputVectorValue *)to_any_value)->vector_array;
      BLI_assert(vector_array.size() == 1);
      GenericVirtualListListRef list_list_ref = this->get_vector_input__single(input);
      vector_array.extend_single__copy(0, list_list_ref[0]);
      return vector_array;
    }

    if (from_any_value->type == ValueType::OwnVector) {
      OwnVectorValue *value = (OwnVectorValue *)from_any_value;
      if (value->max_remaining_users == 1) {
        m_value_per_output_id[to.id()] = value;
        m_value_per_output_id[from.id()] = nullptr;
        value->max_remaining_users = to.target_amount();
        return *value->vector_array;
      }
    }

    GenericVirtualListListRef list_list_ref = this->get_vector_input__single(input);

    GenericVectorArray *new_vector_array = new GenericVectorArray(base_type, 1);
    new_vector_array->extend_single__copy(0, list_list_ref[0]);

    OwnVectorValue *new_value = m_allocator.construct<OwnVectorValue>(*new_vector_array,
                                                                      to.target_amount());
    m_value_per_output_id[to.id()] = new_value;
    return *new_vector_array;
  }

  /* Get readonly inputs for a function call.
   **************************************************/

  GenericVirtualListRef get_single_input__full(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();
    Value *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    if (any_value->type == ValueType::OwnSingle) {
      OwnSingleValue *value = (OwnSingleValue *)any_value;
      if (value->is_single_allocated) {
        return GenericVirtualListRef::FromSingle(
            value->array_ref.type(), value->array_ref.buffer(), m_min_array_size);
      }
      else {
        return value->array_ref;
      }
    }
    else if (any_value->type == ValueType::InputSingle) {
      InputSingleValue *value = (InputSingleValue *)any_value;
      return value->list_ref;
    }
    else if (any_value->type == ValueType::OutputSingle) {
      OutputSingleValue *value = (OutputSingleValue *)any_value;
      BLI_assert(value->is_computed);
      return value->array_ref;
    }

    BLI_assert(false);
    return GenericVirtualListRef(CPPType_float);
  }

  GenericVirtualListRef get_single_input__single(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();
    Value *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    if (any_value->type == ValueType::OwnSingle) {
      OwnSingleValue *value = (OwnSingleValue *)any_value;
      BLI_assert(value->array_ref.size() == 1);
      return value->array_ref;
    }
    else if (any_value->type == ValueType::InputSingle) {
      InputSingleValue *value = (InputSingleValue *)any_value;
      BLI_assert(value->list_ref.is_single_element());
      return value->list_ref;
    }
    else if (any_value->type == ValueType::OutputSingle) {
      OutputSingleValue *value = (OutputSingleValue *)any_value;
      BLI_assert(value->is_computed);
      BLI_assert(value->array_ref.size() == 1);
      return value->array_ref;
    }

    BLI_assert(false);
    return GenericVirtualListRef(CPPType_float);
  }

  GenericVirtualListListRef get_vector_input__full(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();
    Value *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    if (any_value->type == ValueType::OwnVector) {
      OwnVectorValue *value = (OwnVectorValue *)any_value;
      if (value->vector_array->size() == 1) {
        GenericArrayRef array_ref = (*value->vector_array)[0];
        return GenericVirtualListListRef::FromSingleArray(
            array_ref.type(), array_ref.buffer(), array_ref.size(), m_min_array_size);
      }
      else {
        return *value->vector_array;
      }
    }
    else if (any_value->type == ValueType::InputVector) {
      InputVectorValue *value = (InputVectorValue *)any_value;
      return value->list_list_ref;
    }
    else if (any_value->type == ValueType::OutputVector) {
      OutputVectorValue *value = (OutputVectorValue *)any_value;
      return *value->vector_array;
    }

    BLI_assert(false);
    return GenericVirtualListListRef::FromSingleArray(CPPType_float, nullptr, 0, 0);
  }

  GenericVirtualListListRef get_vector_input__single(const MFInputSocket &socket)
  {
    const MFOutputSocket &origin = socket.origin();
    Value *any_value = m_value_per_output_id[origin.id()];
    BLI_assert(any_value != nullptr);

    if (any_value->type == ValueType::OwnVector) {
      OwnVectorValue *value = (OwnVectorValue *)any_value;
      BLI_assert(value->vector_array->size() == 1);
      return *value->vector_array;
    }
    else if (any_value->type == ValueType::InputVector) {
      InputVectorValue *value = (InputVectorValue *)any_value;
      BLI_assert(value->list_list_ref.is_single_list());
      return value->list_list_ref;
    }
    else if (any_value->type == ValueType::OutputVector) {
      OutputVectorValue *value = (OutputVectorValue *)any_value;
      BLI_assert(value->vector_array->size() == 1);
      return *value->vector_array;
    }

    BLI_assert(false);
    return GenericVirtualListListRef::FromSingleArray(CPPType_float, nullptr, 0, 0);
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

  const MFNetwork &network = m_outputs[0]->node().network();
  Storage storage(context.buffer_cache(), mask, network.socket_ids().size());

  Vector<const MFInputSocket *> outputs_to_initialize_in_the_end;

  this->copy_inputs_to_storage(params, storage);
  this->copy_outputs_to_storage(params, storage, outputs_to_initialize_in_the_end);
  this->evaluate_network_to_compute_outputs(context, storage);
  this->initialize_remaining_outputs(params, storage, outputs_to_initialize_in_the_end);
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_inputs_to_storage(MFParams params,
                                                             Storage &storage) const
{
  for (uint input_index : m_inputs.index_range()) {
    uint param_index = input_index + 0;
    const MFOutputSocket &socket = *m_inputs[input_index];
    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef input_list = params.readonly_single_input(param_index);
        storage.add_single_input_from_caller(socket, input_list);
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef input_list_list = params.readonly_vector_input(param_index);
        storage.add_vector_input_from_caller(socket, input_list_list);
        break;
      }
    }
  }
}

BLI_NOINLINE void MF_EvaluateNetwork::copy_outputs_to_storage(
    MFParams params,
    Storage &storage,
    Vector<const MFInputSocket *> &outputs_to_initialize_in_the_end) const
{
  for (uint output_index : m_outputs.index_range()) {
    uint param_index = output_index + m_inputs.size();
    const MFInputSocket &socket = *m_outputs[output_index];
    const MFOutputSocket &origin = socket.origin();

    if (origin.node().is_dummy()) {
      BLI_assert(m_inputs.contains(&origin));
      /* Don't overwrite input buffers. */
      outputs_to_initialize_in_the_end.append(&socket);
      continue;
    }

    if (storage.socket_has_buffer_for_output(origin)) {
      /* When two outputs will be initialized to the same values. */
      outputs_to_initialize_in_the_end.append(&socket);
      continue;
    }

    switch (socket.data_type().category()) {
      case MFDataType::Single: {
        GenericMutableArrayRef array_ref = params.uninitialized_single_output(param_index);
        storage.add_single_output_from_caller(origin, array_ref);
        break;
      }
      case MFDataType::Vector: {
        GenericVectorArray &vector_array = params.vector_output(param_index);
        storage.add_vector_output_from_caller(origin, vector_array);
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

  ScopedVector<const MFOutputSocket *> missing_sockets;

  while (!sockets_to_compute.is_empty()) {
    const MFOutputSocket &socket = *sockets_to_compute.peek();
    const MFNode &node = socket.node();

    if (storage.socket_is_computed(socket)) {
      sockets_to_compute.pop();
      continue;
    }

    BLI_assert(node.is_function());
    const MFFunctionNode &function_node = node.as_function();

    missing_sockets.clear();
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

  if (this->can_do_single_value_evaluation(function_node, storage)) {
    MFParamsBuilder params_builder{function, 1};

    for (uint param_index : function.param_indices()) {
      MFParamType param_type = function.param_type(param_index);
      switch (param_type.type()) {
        case MFParamType::SingleInput: {
          const MFInputSocket &socket = function_node.input_for_param(param_index);
          GenericVirtualListRef values = storage.get_single_input__single(socket);
          params_builder.add_readonly_single_input(values);
          break;
        }
        case MFParamType::VectorInput: {
          const MFInputSocket &socket = function_node.input_for_param(param_index);
          GenericVirtualListListRef values = storage.get_vector_input__single(socket);
          params_builder.add_readonly_vector_input(values);
          break;
        }
        case MFParamType::SingleOutput: {
          const MFOutputSocket &socket = function_node.output_for_param(param_index);
          GenericMutableArrayRef values = storage.get_single_output__single(socket);
          params_builder.add_single_output(values);
          break;
        }
        case MFParamType::VectorOutput: {
          const MFOutputSocket &socket = function_node.output_for_param(param_index);
          GenericVectorArray &values = storage.get_vector_output__single(socket);
          params_builder.add_vector_output(values);
          break;
        }
        case MFParamType::MutableSingle: {
          const MFInputSocket &input = function_node.input_for_param(param_index);
          const MFOutputSocket &output = function_node.output_for_param(param_index);
          GenericMutableArrayRef values = storage.get_mutable_single__single(input, output);
          params_builder.add_mutable_single(values);
          break;
        }
        case MFParamType::MutableVector: {
          const MFInputSocket &input = function_node.input_for_param(param_index);
          const MFOutputSocket &output = function_node.output_for_param(param_index);
          GenericVectorArray &values = storage.get_mutable_vector__single(input, output);
          params_builder.add_mutable_vector(values);
          break;
        }
      }
    }

    function.call(IndexRange(1), params_builder, global_context);
  }
  else {
    MFParamsBuilder params_builder{function, storage.mask().min_array_size()};

    for (uint param_index : function.param_indices()) {
      MFParamType param_type = function.param_type(param_index);
      switch (param_type.type()) {
        case MFParamType::SingleInput: {
          const MFInputSocket &socket = function_node.input_for_param(param_index);
          GenericVirtualListRef values = storage.get_single_input__full(socket);
          params_builder.add_readonly_single_input(values);
          break;
        }
        case MFParamType::VectorInput: {
          const MFInputSocket &socket = function_node.input_for_param(param_index);
          GenericVirtualListListRef values = storage.get_vector_input__full(socket);
          params_builder.add_readonly_vector_input(values);
          break;
        }
        case MFParamType::SingleOutput: {
          const MFOutputSocket &socket = function_node.output_for_param(param_index);
          GenericMutableArrayRef values = storage.get_single_output__full(socket);
          params_builder.add_single_output(values);
          break;
        }
        case MFParamType::VectorOutput: {
          const MFOutputSocket &socket = function_node.output_for_param(param_index);
          GenericVectorArray &values = storage.get_vector_output__full(socket);
          params_builder.add_vector_output(values);
          break;
        }
        case MFParamType::MutableSingle: {
          const MFInputSocket &input = function_node.input_for_param(param_index);
          const MFOutputSocket &output = function_node.output_for_param(param_index);
          GenericMutableArrayRef values = storage.get_mutable_single__full(input, output);
          params_builder.add_mutable_single(values);
          break;
        }
        case MFParamType::MutableVector: {
          const MFInputSocket &input = function_node.input_for_param(param_index);
          const MFOutputSocket &output = function_node.output_for_param(param_index);
          GenericVectorArray &values = storage.get_mutable_vector__full(input, output);
          params_builder.add_mutable_vector(values);
          break;
        }
      }
    }

    function.call(storage.mask(), params_builder, global_context);
  }

  for (const MFInputSocket *socket : function_node.inputs()) {
    storage.finish_input_socket(*socket);
  }
  for (const MFOutputSocket *socket : function_node.outputs()) {
    storage.finish_output_socket(*socket);
  }
}

bool MF_EvaluateNetwork::can_do_single_value_evaluation(const MFFunctionNode &function_node,
                                                        Storage &storage) const
{
  if (function_node.function().depends_on_per_element_context()) {
    return false;
  }
  for (const MFInputSocket *socket : function_node.inputs()) {
    if (!storage.is_same_value_for_every_index(socket->origin())) {
      return false;
    }
  }
  if (storage.mask().min_array_size() >= 1) {
    for (const MFOutputSocket *socket : function_node.outputs()) {
      if (storage.socket_has_buffer_for_output(*socket)) {
        return false;
      }
    }
  }
  return true;
}

BLI_NOINLINE void MF_EvaluateNetwork::initialize_remaining_outputs(
    MFParams params, Storage &storage, ArrayRef<const MFInputSocket *> remaining_outputs) const
{
  for (const MFInputSocket *socket : remaining_outputs) {
    uint param_index = m_inputs.size() + m_outputs.index(socket);

    switch (socket->data_type().category()) {
      case MFDataType::Single: {
        GenericVirtualListRef values = storage.get_single_input__full(*socket);
        GenericMutableArrayRef output_values = params.uninitialized_single_output(param_index);
        values.materialize_to_uninitialized(storage.mask(), output_values);
        break;
      }
      case MFDataType::Vector: {
        GenericVirtualListListRef values = storage.get_vector_input__full(*socket);
        GenericVectorArray &output_values = params.vector_output(param_index);
        output_values.extend_multiple__copy(storage.mask(), values);
        break;
      }
    }
  }
}

}  // namespace FN
