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
    this->set_signature(signature);
  }

  class Storage {
   private:
    MFMask m_mask;
    Vector<GenericVectorArray *> m_vector_arrays;
    Vector<GenericMutableArrayRef> m_arrays;
    Map<uint, GenericVectorArray *> m_vector_array_for_inputs;
    Map<uint, GenericVirtualListRef> m_virtual_list_for_inputs;
    Map<uint, GenericVirtualListListRef> m_virtual_list_list_for_inputs;
    Map<uint, GenericMutableArrayRef> m_array_ref_for_inputs;

   public:
    Storage(MFMask mask) : m_mask(mask)
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

    GenericMutableArrayRef allocate_array(const CPPType &type)
    {
      uint size = m_mask.min_array_size();
      void *buffer = MEM_malloc_arrayN(size, type.size(), __func__);
      GenericMutableArrayRef array(type, buffer, size);
      m_arrays.append(array);
      return array;
    }

    GenericVectorArray &allocate_vector_array(const CPPType &type)
    {
      uint size = m_mask.min_array_size();
      GenericVectorArray *vector_array = new GenericVectorArray(type, size);
      m_vector_arrays.append(vector_array);
      return *vector_array;
    }

    void set_array_ref_for_input(const MFInputSocket &socket, GenericMutableArrayRef array)
    {
      m_array_ref_for_inputs.add_new(socket.id(), array);
    }

    void set_virtual_list_for_input(const MFInputSocket &socket, GenericVirtualListRef list)
    {
      m_virtual_list_for_inputs.add_new(socket.id(), list);
    }

    void set_virtual_list_list_for_input(const MFInputSocket &socket,
                                         GenericVirtualListListRef list)
    {
      m_virtual_list_list_for_inputs.add_new(socket.id(), list);
    }

    void set_vector_array_for_input(const MFInputSocket &socket, GenericVectorArray &vector_array)
    {
      m_vector_array_for_inputs.add_new(socket.id(), &vector_array);
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
      return *m_vector_array_for_inputs.lookup(socket.id());
    }

    GenericMutableArrayRef get_array_ref_for_input(const MFInputSocket &socket) const
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
  };

  void call(MFMask mask, MFParams params, MFContext context) const override;

 private:
  void copy_inputs_to_storage(MFMask mask, MFParams params, Storage &storage) const;

  void evaluate_network_to_compute_outputs(MFMask mask,
                                           MFContext &global_context,
                                           Storage &storage) const;

  void compute_and_forward_outputs(MFMask mask,
                                   MFContext &global_context,
                                   const MFFunctionNode &function_node,
                                   Storage &storage) const;

  void copy_computed_values_to_outputs(MFMask mask, MFParams params, Storage &storage) const;
};

}  // namespace FN
