#include "FN_tuple_call.hpp"

#include "array_execution.hpp"

namespace FN {
namespace Functions {

TupleCallArrayExecution::TupleCallArrayExecution(SharedFunction function)
    : m_function(std::move(function))
{
  BLI_assert(m_function->has_body<TupleCallBody>());
  for (SharedType &type : m_function->input_types()) {
    m_input_sizes.append(type->extension<CPPTypeInfo>().size());
  }
  for (SharedType &type : m_function->output_types()) {
    m_output_sizes.append(type->extension<CPPTypeInfo>().size());
  }
}

void TupleCallArrayExecution::call(ArrayRef<uint> indices,
                                   ArrayRef<void *> input_buffers,
                                   ArrayRef<void *> output_buffers,
                                   ExecutionContext &execution_context)
{
  uint input_amount = m_function->input_amount();
  uint output_amount = m_function->output_amount();

  BLI_assert(input_amount == input_buffers.size());
  BLI_assert(output_amount == output_buffers.size());

  TupleCallBody &body = m_function->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

  for (uint index : indices) {
    for (uint i = 0; i < input_amount; i++) {
      void *ptr = POINTER_OFFSET(input_buffers[i], index * m_input_sizes[i]);
      fn_in.copy_in__dynamic(i, ptr);
    }

    body.call(fn_in, fn_out, execution_context);

    for (uint i = 0; i < output_amount; i++) {
      void *ptr = POINTER_OFFSET(output_buffers[i], index * m_output_sizes[i]);
      fn_out.relocate_out__dynamic(i, ptr);
    }
  }
}

}  // namespace Functions
}  // namespace FN
