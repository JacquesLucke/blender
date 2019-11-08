#pragma once

#include "FN_multi_function_network.h"

#include "BLI_map.h"
#include "BLI_stack_cxx.h"

namespace FN {

using BLI::Map;
using BLI::Stack;

class MF_EvaluateNetwork final : public MultiFunction {
 private:
  Vector<const MFOutputSocket *> m_inputs;
  Vector<const MFInputSocket *> m_outputs;

 public:
  MF_EvaluateNetwork(Vector<const MFOutputSocket *> inputs, Vector<const MFInputSocket *> outputs)
      : m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
    MFSignatureBuilder signature("Function Tree");
    for (auto socket : m_inputs) {
      BLI_assert(socket->node().is_dummy());

      MFDataType type = socket->type();
      switch (type.category()) {
        case MFDataType::Single:
          signature.readonly_single_input("Input", type.type());
          break;
        case MFDataType::Vector:
          signature.readonly_vector_input("Input", type.base_type());
          break;
      }
    }
    for (auto socket : m_outputs) {
      BLI_assert(socket->node().is_dummy());

      MFDataType type = socket->type();
      switch (type.category()) {
        case MFDataType::Single:
          signature.single_output("Output", type.type());
          break;
        case MFDataType::Vector:
          signature.vector_output("Output", type.base_type());
          break;
      }
    }
    this->set_signature(signature);
  }

  class Storage {
   private:
    const MFMask &m_mask;
    Vector<GenericVectorArray *> m_vector_arrays;
    Vector<GenericMutableArrayRef> m_arrays;
    Map<uint, GenericVectorArray *> m_vector_per_socket;
    Map<uint, GenericVirtualListRef> m_virtual_list_for_inputs;
    Map<uint, GenericVirtualListListRef> m_virtual_list_list_for_inputs;

   public:
    Storage(const MFMask &mask) : m_mask(mask)
    {
    }

    ~Storage()
    {
      for (GenericVectorArray *vector_array : m_vector_arrays) {
        delete vector_array;
      }
      for (GenericMutableArrayRef array : m_arrays) {
        array.destruct_indices(m_mask.indices());
        MEM_freeN(array.buffer());
      }
    }

    void take_array_ref_ownership(GenericMutableArrayRef array)
    {
      m_arrays.append(array);
    }

    void take_vector_array_ownership(GenericVectorArray *vector_array)
    {
      m_vector_arrays.append(vector_array);
    }

    void take_vector_array_ownership__not_twice(GenericVectorArray *vector_array)
    {
      if (!m_vector_arrays.contains(vector_array)) {
        m_vector_arrays.append(vector_array);
      }
    }

    void set_virtual_list_for_input__non_owning(const MFInputSocket &socket,
                                                GenericVirtualListRef list)
    {
      m_virtual_list_for_inputs.add_new(socket.id(), list);
    }

    void set_virtual_list_list_for_input__non_owning(const MFInputSocket &socket,
                                                     GenericVirtualListListRef list)
    {
      m_virtual_list_list_for_inputs.add_new(socket.id(), list);
    }

    void set_vector_array_for_input__non_owning(const MFInputSocket &socket,
                                                GenericVectorArray *vector_array)
    {
      m_vector_per_socket.add_new(socket.id(), vector_array);
    }

    GenericVirtualListRef get_virtual_list_for_input(const MFInputSocket &socket) const
    {
      return m_virtual_list_for_inputs.lookup(socket.id());
    }

    GenericVirtualListListRef get_virtual_list_list_for_input(const MFInputSocket &socket) const
    {
      return m_virtual_list_list_for_inputs.lookup(socket.id());
    }

    GenericVectorArray &get_vector_array_for_input(const MFInputSocket &socket) const
    {
      return *m_vector_per_socket.lookup(socket.id());
    }

    bool input_is_computed(const MFInputSocket &socket) const
    {
      switch (socket.type().category()) {
        case MFDataType::Single:
          return m_virtual_list_for_inputs.contains(socket.id());
        case MFDataType::Vector:
          return m_virtual_list_list_for_inputs.contains(socket.id()) ||
                 m_vector_per_socket.contains(socket.id());
      }
      BLI_assert(false);
      return false;
    }
  };

  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;

 private:
  void copy_inputs_to_storage(MFParams &params, Storage &storage) const;

  void evaluate_network_to_compute_outputs(const MFMask &mask,
                                           MFContext &global_context,
                                           Storage &storage) const;

  void compute_and_forward_outputs(const MFMask &mask,
                                   MFContext &global_context,
                                   const MFFunctionNode &function_node,
                                   Storage &storage) const;

  void copy_computed_values_to_outputs(const MFMask &mask,
                                       MFParams &params,
                                       Storage &storage) const;

  GenericMutableArrayRef allocate_array(const CPPType &type, uint size) const
  {
    void *buffer = MEM_malloc_arrayN(size, type.size(), __func__);
    return GenericMutableArrayRef(type, buffer, size);
  }
};

}  // namespace FN
