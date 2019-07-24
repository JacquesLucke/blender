#pragma once

#include "FN_tuple_call.hpp"
#include "attributes.hpp"
#include "action_interface.hpp"
#include "force_interface.hpp"

namespace BParticles {

using BLI::ArrayRef;
using BLI::Vector;
using FN::CPPTypeInfo;
using FN::ExecutionContext;
using FN::ExecutionStack;
using FN::SharedFunction;
using FN::SharedType;
using FN::TupleCallBody;

class ParticleFunction;

class ParticleFunctionCaller {
 private:
  TupleCallBody *m_body;
  uint m_min_buffer_length;
  Vector<void *> m_input_buffers;
  Vector<void *> m_output_buffers;
  Vector<uint> m_input_strides;
  Vector<uint> m_output_strides;
  ArrayAllocator *m_array_allocator;

  friend ParticleFunction;

 public:
  template<typename T> void add_output(ArrayRef<T> array, StringRef expected_name)
  {
#ifdef DEBUG
    uint index = m_output_buffers.size();
    SharedType &type = m_body->owner()->output_type(index);
    uint expected_stride = type->extension<CPPTypeInfo>()->size_of_type();
    uint given_stride = sizeof(T);
    BLI_assert(expected_stride == given_stride);
    BLI_assert(m_min_buffer_length <= array.size());
    StringRef real_name = m_body->owner()->output_name(index);
    BLI_assert(expected_name == real_name);
#endif

    m_output_buffers.append((void *)array.begin());
    m_output_strides.append(sizeof(T));
  }

  template<typename T> ArrayAllocator::Array<T> add_output(StringRef expected_name)
  {
    ArrayAllocator::Array<T> array(*m_array_allocator);
    this->add_output(array.as_array_ref(), expected_name);
    return array;
  }

  void call(ArrayRef<uint> pindices)
  {
    BLI_assert(m_body->owner()->input_amount() == m_input_buffers.size());
    BLI_assert(m_body->owner()->output_amount() == m_output_buffers.size());

    ExecutionStack stack;
    ExecutionContext execution_context(stack);

    FN_TUPLE_CALL_ALLOC_TUPLES(m_body, fn_in, fn_out);

    for (uint pindex : pindices) {
      for (uint i = 0; i < m_input_buffers.size(); i++) {
        void *ptr = POINTER_OFFSET(m_input_buffers[i], pindex * m_input_strides[i]);
        fn_in.copy_in__dynamic(i, ptr);
      }

      m_body->call(fn_in, fn_out, execution_context);

      for (uint i = 0; i < m_output_buffers.size(); i++) {
        void *ptr = POINTER_OFFSET(m_output_buffers[i], pindex * m_output_strides[i]);
        fn_out.relocate_out__dynamic(i, ptr);
      }
    }
  }
};

class ParticleFunction {
 private:
  SharedFunction m_fn;
  TupleCallBody *m_body;

 public:
  ParticleFunction(SharedFunction fn) : m_fn(std::move(fn)), m_body(m_fn->body<TupleCallBody>())
  {
    BLI_assert(m_body != nullptr);
  }

  ParticleFunctionCaller get_caller(ActionInterface &action_interface);
  ParticleFunctionCaller get_caller(OffsetHandlerInterface &offset_handler_interface);
  ParticleFunctionCaller get_caller(ForceInterface &force_interface);

 private:
  ParticleFunctionCaller get_caller(ArrayAllocator &array_allocator,
                                    AttributeArrays attributes,
                                    ActionContext *action_context);
};

}  // namespace BParticles
