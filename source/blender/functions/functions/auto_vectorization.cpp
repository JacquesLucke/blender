#include <cmath>

#include "FN_functions.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Functions {

template<typename T> uint get_list_length(const Types::List<T> *list)
{
  return list->size();
}

static llvm::Value *build_ir__get_list_length(CodeBuilder &builder,
                                              SharedType &base_type,
                                              llvm::Value *list,
                                              CodeInterface &parent_interface,
                                              const BuildIRSettings &settings)
{
  if (base_type == Types::GET_TYPE_float()) {
    return builder.CreateCallPointer((void *)get_list_length<float>, {list}, builder.getInt32Ty());
  }
  else if (base_type == Types::GET_TYPE_fvec3()) {
    return builder.CreateCallPointer(
        (void *)get_list_length<Types::Vector>, {list}, builder.getInt32Ty());
  }
  else {
    BLI_assert(!"not yet supported");
    return nullptr;
  }

  SharedFunction &get_length_fn = GET_FN_list_length(base_type);
  if (!get_length_fn->has_body<LLVMBuildIRBody>()) {
    derive_LLVMBuildIRBody_from_TupleCallBody(get_length_fn);
  }

  auto *body = get_length_fn->body<LLVMBuildIRBody>();
  LLVMValues inputs = {list};
  LLVMValues outputs(1);
  CodeInterface interface(
      inputs, outputs, parent_interface.context_ptr(), parent_interface.function_ir_cache());
  body->build_ir(builder, interface, settings);
  return outputs[0];
}

template<typename T> T *get_value_ptr(const Types::List<T> *list)
{
  return list->data_ptr();
}

static llvm::Value *build_ir__get_list_value_ptr(CodeBuilder &builder,
                                                 SharedType &base_type,
                                                 llvm::Value *list)
{
  if (base_type == Types::GET_TYPE_float()) {
    return builder.CreateCallPointer_RetVoidPtr((void *)get_value_ptr<float>, {list});
  }
  else if (base_type == Types::GET_TYPE_fvec3()) {
    return builder.CreateCallPointer_RetVoidPtr((void *)get_value_ptr<Types::Vector>, {list});
  }
  else {
    BLI_assert(!"not yet supported");
    return nullptr;
  }
}

template<typename T> Types::List<T> *new_list_with_prepared_memory(uint length)
{
  auto *list = new Types::List<T>(length);
  return list;
}

static llvm::Value *build_ir__new_list_with_prepared_memory(CodeBuilder &builder,
                                                            SharedType &base_type,
                                                            llvm::Value *length)
{
  LLVMValues args = {length};
  if (base_type == Types::GET_TYPE_float()) {
    return builder.CreateCallPointer_RetVoidPtr((void *)new_list_with_prepared_memory<float>,
                                                args);
  }
  else if (base_type == Types::GET_TYPE_fvec3()) {
    return builder.CreateCallPointer_RetVoidPtr(
        (void *)new_list_with_prepared_memory<Types::Vector>, args);
  }
  else {
    BLI_assert(!"not yet supported");
    return nullptr;
  }
}

class AutoVectorizationGen : public LLVMBuildIRBody {
 private:
  SharedFunction m_main;
  SmallVector<bool> m_input_is_list;
  SmallVector<uint> m_list_inputs;

 public:
  AutoVectorizationGen(SharedFunction main, const SmallVector<bool> &input_is_list)
      : m_main(main), m_input_is_list(input_is_list)
  {
    for (uint i = 0; i < input_is_list.size(); i++) {
      if (input_is_list[i]) {
        m_list_inputs.append(i);
      }
    }

    BLI_assert(m_list_inputs.size() >= 1);
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &settings) const override
  {
    LLVMValues list_lengths;
    Signature &main_sig = m_main->signature();

    for (uint index : m_list_inputs) {
      llvm::Value *length = build_ir__get_list_length(
          builder, this->input_type(index), interface.get_input(index), interface, settings);
      list_lengths.append(length);
    }

    llvm::Value *max_length = builder.CreateSIntMax(list_lengths);

    LLVMValues input_data_pointers;
    for (uint index : m_list_inputs) {
      SharedType &type = this->input_type(index);
      auto *cpp_type_info = type->extension<CPPTypeInfo>();
      llvm::Type *stride_type = builder.getFixedSizeType(cpp_type_info->size_of_type());

      llvm::Value *data_ptr = build_ir__get_list_value_ptr(
          builder, type, interface.get_input(index));
      llvm::Value *typed_data_ptr = builder.CastToPointerOf(data_ptr, stride_type);

      input_data_pointers.append(typed_data_ptr);
    }

    LLVMValues output_data_pointers;
    for (uint i = 0; i < main_sig.outputs().size(); i++) {
      SharedType &type = main_sig.outputs()[i].type();
      auto *cpp_type_info = type->extension<CPPTypeInfo>();
      llvm::Type *stride_type = builder.getFixedSizeType(cpp_type_info->size_of_type());

      llvm::Value *output_list = build_ir__new_list_with_prepared_memory(
          builder, type, max_length);
      llvm::Value *data_ptr = build_ir__get_list_value_ptr(builder, type, output_list);
      llvm::Value *typed_data_ptr = builder.CastToPointerOf(data_ptr, stride_type);

      output_data_pointers.append(typed_data_ptr);
      interface.set_output(i, output_list);
    }

    auto *setup_block = builder.GetInsertBlock();
    auto *condition_block = builder.NewBlockInFunction("Loop Condition");
    auto *body_block = builder.NewBlockInFunction("Loop Body");
    auto *end_block = builder.NewBlockInFunction("Loop End");

    builder.CreateBr(condition_block);

    CodeBuilder body_builder(body_block);
    CodeBuilder condition_builder(condition_block);
    CodeBuilder end_builder(end_block);

    auto *iteration = condition_builder.CreatePhi(condition_builder.getInt32Ty(), 2);
    auto *condition = condition_builder.CreateICmpULT(iteration, max_length);
    condition_builder.CreateCondBr(condition, body_block, end_block);

    LLVMValues main_inputs;

    uint list_input_index = 0;
    for (uint i = 0; i < main_sig.inputs().size(); i++) {
      SharedType &type = this->input_type(i);
      auto *llvm_type_info = type->extension<LLVMTypeInfo>();
      BLI_assert(llvm_type_info);
      if (m_input_is_list[i]) {

        llvm::Value *load_address = body_builder.CreateGEP(input_data_pointers[list_input_index],
                                                           iteration);
        // TODO: handle different lengths
        llvm::Value *value_for_main = llvm_type_info->build_load_ir__relocate(body_builder,
                                                                              load_address);
        main_inputs.append(value_for_main);
        list_input_index++;
      }
      else {
        llvm::Value *source_value = interface.get_input(i);
        llvm::Value *value_for_main = llvm_type_info->build_copy_ir(body_builder, source_value);
        main_inputs.append(value_for_main);
      }
    }

    LLVMValues main_outputs(main_sig.outputs().size());
    CodeInterface main_interface(
        main_inputs, main_outputs, interface.context_ptr(), interface.function_ir_cache());
    auto *body = m_main->body<LLVMBuildIRBody>();
    body->build_ir(body_builder, main_interface, settings);

    for (uint i = 0; i < main_sig.outputs().size(); i++) {
      SharedType &type = main_sig.outputs()[i].type();
      auto *type_info = type->extension<LLVMTypeInfo>();
      llvm::Value *store_address = body_builder.CreateGEP(output_data_pointers[i], iteration);
      llvm::Value *computed_value = main_outputs[i];
      type_info->build_store_ir__relocate(body_builder, computed_value, store_address);
    }

    llvm::Value *next_iteration = body_builder.CreateIAdd(iteration, body_builder.getInt32(1));
    body_builder.CreateBr(condition_block);

    iteration->addIncoming(condition_builder.getInt32(0), setup_block);
    iteration->addIncoming(next_iteration, body_block);

    builder.SetInsertPoint(end_block);

    for (uint i = 0; i < m_list_inputs.size(); i++) {
      uint index = m_list_inputs[i];
      SharedType &base_type = main_sig.inputs()[index].type();
      SharedType &list_type = get_list_type(base_type);
      auto *type_info = list_type->extension<LLVMTypeInfo>();
      llvm::Value *input_list = interface.get_input(index);
      type_info->build_free_ir(builder, input_list);
    }
  }

 private:
  SharedType &input_type(uint index) const
  {
    return m_main->signature().inputs()[index].type();
  }
};

class AutoVectorization : public TupleCallBody {
 private:
  SharedFunction m_main;
  TupleCallBody *m_main_body;

  const SmallVector<bool> m_input_is_list;
  SmallVector<uint> m_list_inputs;

  SmallVector<TupleCallBody *> m_get_length_bodies;
  uint m_max_len_in_size, m_max_len_out_size;

  SmallVector<TupleCallBody *> m_get_element_bodies;
  SmallVector<TupleCallBody *> m_create_empty_bodies;
  SmallVector<TupleCallBody *> m_append_bodies;

 public:
  AutoVectorization(SharedFunction main, const SmallVector<bool> &input_is_list)
      : m_main(main), m_main_body(main->body<TupleCallBody>()), m_input_is_list(input_is_list)
  {
    for (uint i = 0; i < input_is_list.size(); i++) {
      if (input_is_list[i]) {
        m_list_inputs.append(i);
      }
    }
    for (uint i : m_list_inputs) {
      SharedType &base_type = main->signature().inputs()[i].type();
      m_get_length_bodies.append(GET_FN_list_length(base_type)->body<TupleCallBody>());
      m_get_element_bodies.append(GET_FN_get_list_element(base_type)->body<TupleCallBody>());
    }

    m_max_len_in_size = 0;
    m_max_len_out_size = 0;
    for (TupleCallBody *body : m_get_length_bodies) {
      m_max_len_in_size = std::max(m_max_len_in_size, body->meta_in()->size_of_full_tuple());
      m_max_len_out_size = std::max(m_max_len_out_size, body->meta_out()->size_of_full_tuple());
    }

    for (auto output : main->signature().outputs()) {
      SharedType &base_type = output.type();
      m_create_empty_bodies.append(GET_FN_empty_list(base_type)->body<TupleCallBody>());
      m_append_bodies.append(GET_FN_append_to_list(base_type)->body<TupleCallBody>());
    }
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    uint *input_lengths = BLI_array_alloca(input_lengths, m_list_inputs.size());
    this->get_input_list_lengths(fn_in, ctx, input_lengths);
    uint max_length = *std::max_element(input_lengths, input_lengths + m_list_inputs.size());

    this->initialize_empty_lists(fn_out, ctx);

    FN_TUPLE_CALL_ALLOC_TUPLES(m_main_body, main_in, main_out);

    for (uint iteration = 0; iteration < max_length; iteration++) {
      uint list_index = 0;
      for (uint i = 0; i < m_input_is_list.size(); i++) {
        if (m_input_is_list[i]) {
          this->copy_in_iteration(
              iteration, fn_in, main_in, i, list_index, input_lengths[list_index], ctx);
          list_index++;
        }
        else {
          Tuple::copy_element(fn_in, i, main_in, i);
        }
      }

      m_main_body->call__setup_stack(main_in, main_out, ctx);

      for (uint i = 0; i < m_main->signature().outputs().size(); i++) {
        this->append_to_output(main_out, fn_out, i, ctx);
      }
    }
  }

 private:
  void get_input_list_lengths(Tuple &fn_in, ExecutionContext &ctx, uint *r_lengths) const
  {
    for (uint i = 0; i < m_list_inputs.size(); i++) {
      uint index_in_tuple = m_list_inputs[i];
      r_lengths[i] = this->get_input_list_length(fn_in, index_in_tuple, i, ctx);
    }
  }

  uint get_input_list_length(Tuple &fn_in,
                             uint index_in_tuple,
                             uint list_index,
                             ExecutionContext &ctx) const
  {
    TupleCallBody *body = m_get_length_bodies[list_index];
    FN_TUPLE_CALL_ALLOC_TUPLES(body, get_length_in, get_length_out);

    Tuple::copy_element(fn_in, index_in_tuple, get_length_in, 0);
    body->call__setup_stack(get_length_in, get_length_out, ctx);
    uint length = get_length_out.get<uint>(0);
    return length;
  }

  void copy_in_iteration(uint iteration,
                         Tuple &fn_in,
                         Tuple &main_in,
                         uint index,
                         uint list_index,
                         uint list_length,
                         ExecutionContext &ctx) const
  {
    if (list_length == 0) {
      main_in.init_default(index);
      return;
    }
    uint load_index = iteration % list_length;

    TupleCallBody *body = m_get_element_bodies[list_index];
    FN_TUPLE_CALL_ALLOC_TUPLES(body, get_element_in, get_element_out);

    Tuple::copy_element(fn_in, index, get_element_in, 0);
    get_element_in.set<uint>(1, load_index);
    get_element_in.init_default(2);
    body->call__setup_stack(get_element_in, get_element_out, ctx);
    Tuple::relocate_element(get_element_out, 0, main_in, index);
  }

  void initialize_empty_lists(Tuple &fn_out, ExecutionContext &ctx) const
  {
    for (uint i = 0; i < m_main->signature().outputs().size(); i++) {
      this->initialize_empty_list(fn_out, i, ctx);
    }
  }

  void initialize_empty_list(Tuple &fn_out, uint index, ExecutionContext &ctx) const
  {
    TupleCallBody *body = m_create_empty_bodies[index];
    FN_TUPLE_CALL_ALLOC_TUPLES(body, create_list_in, create_list_out);

    body->call__setup_stack(create_list_in, create_list_out, ctx);
    Tuple::relocate_element(create_list_out, 0, fn_out, index);
  }

  void append_to_output(Tuple &main_out, Tuple &fn_out, uint index, ExecutionContext &ctx) const
  {
    TupleCallBody *body = m_append_bodies[index];
    FN_TUPLE_CALL_ALLOC_TUPLES(body, append_in, append_out);

    Tuple::relocate_element(fn_out, index, append_in, 0);
    Tuple::relocate_element(main_out, index, append_in, 1);
    body->call__setup_stack(append_in, append_out, ctx);
    Tuple::relocate_element(append_out, 0, fn_out, index);
  }
};

static bool any_true(const SmallVector<bool> &list)
{
  for (bool value : list) {
    if (value) {
      return true;
    }
  }
  return false;
}

SharedFunction to_vectorized_function(SharedFunction &original_fn,
                                      const SmallVector<bool> &vectorize_input)
{
  uint input_amount = original_fn->signature().inputs().size();
  uint output_amount = original_fn->signature().outputs().size();

  BLI_assert(vectorize_input.size() == input_amount);
  BLI_assert(any_true(vectorize_input));

  if (!original_fn->has_body<TupleCallBody>()) {
    if (original_fn->has_body<LLVMBuildIRBody>()) {
      derive_TupleCallBody_from_LLVMBuildIRBody(original_fn, *(new llvm::LLVMContext()));
    }
    else {
      BLI_assert(false);
    }
  }

  InputParameters inputs;
  for (uint i = 0; i < input_amount; i++) {
    auto original_parameter = original_fn->signature().inputs()[i];
    if (vectorize_input[i]) {
      SharedType &list_type = get_list_type(original_parameter.type());
      inputs.append(InputParameter(original_parameter.name() + " (List)", list_type));
    }
    else {
      inputs.append(original_parameter);
    }
  }

  OutputParameters outputs;
  for (uint i = 0; i < output_amount; i++) {
    auto original_parameter = original_fn->signature().outputs()[i];
    SharedType &list_type = get_list_type(original_parameter.type());
    outputs.append(OutputParameter(original_parameter.name() + " (List)", list_type));
  }

  std::string name = original_fn->name() + " (Vectorized)";
  auto fn = SharedFunction::New(name, Signature(inputs, outputs));
  fn->add_body(new AutoVectorization(original_fn, vectorize_input));
  // fn->add_body(new AutoVectorizationGen(original_fn, vectorize_input));
  return fn;
}

}  // namespace Functions
}  // namespace FN
