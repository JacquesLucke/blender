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
  MF_EvaluateNetwork(Vector<const MFOutputSocket *> inputs, Vector<const MFInputSocket *> outputs);

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

    MFMask &mask()
    {
      return m_mask;
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

    GenericMutableArrayRef allocate_copy(GenericVirtualListRef array)
    {
      GenericMutableArrayRef new_array = this->allocate_array(array.type());
      for (uint i : m_mask.indices()) {
        new_array.copy_in__uninitialized(i, array[i]);
      }
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
  void copy_inputs_to_storage(MFParams params, Storage &storage) const;

  void evaluate_network_to_compute_outputs(MFContext &global_context, Storage &storage) const;

  void compute_and_forward_outputs(MFContext &global_context,
                                   const MFFunctionNode &function_node,
                                   Storage &storage) const;

  void copy_computed_values_to_outputs(MFParams params, Storage &storage) const;
};

}  // namespace FN
