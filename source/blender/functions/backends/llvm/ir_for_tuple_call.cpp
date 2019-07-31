#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

namespace FN {

static void run_TupleCallBody(TupleCallBody *body,
                              void *data_in,
                              void *data_out,
                              ExecutionContext *ctx)
{
  bool *initialized_in = (bool *)alloca(body->meta_in()->element_amount());
  bool *initialized_out = (bool *)alloca(body->meta_out()->element_amount());

  Tuple fn_in(body->meta_in().ref(), data_in, initialized_in, false);
  Tuple fn_out(body->meta_out().ref(), data_out, initialized_out, false);

  fn_in.set_all_initialized();

  TextStackFrame frame("IR for Tuple Call Wrapper");
  body->call__setup_stack(fn_in, fn_out, *ctx, frame);

  /* This way the data is not freed with the tuples. */
  fn_out.set_all_uninitialized();
}

static void run__setup_ExecutionContext_in_buffer(void *stack_ptr, void *ctx_ptr)
{
  auto *stack = new (stack_ptr) ExecutionStack();
  new (ctx_ptr) ExecutionContext(*stack);
}

static llvm::Value *build__stack_allocate_ExecutionContext(CodeBuilder &builder)
{
  llvm::Value *stack_ptr = builder.CreateAllocaBytes_AnyPtr(sizeof(ExecutionStack));
  llvm::Value *ctx_ptr = builder.CreateAllocaBytes_AnyPtr(sizeof(ExecutionContext));

  builder.CreateCallPointer((void *)run__setup_ExecutionContext_in_buffer,
                            {stack_ptr, ctx_ptr},
                            builder.getVoidTy(),
                            "Setup execution context in buffer");

  return ctx_ptr;
}

class TupleCallLLVM : public LLVMBuildIRBody {
 private:
  TupleCallBody &m_tuple_call;

 public:
  TupleCallLLVM(TupleCallBody &tuple_call) : m_tuple_call(tuple_call)
  {
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &settings) const override
  {
    Function *fn = m_tuple_call.owner();

    /* Find relevant type information. */
    auto input_type_infos = fn->input_extensions<LLVMTypeInfo>();
    auto output_type_infos = fn->output_extensions<LLVMTypeInfo>();

    /* Build wrapper function. */
    llvm::Function *wrapper_function = this->get_wrapper_function(
        builder, interface, settings, input_type_infos, output_type_infos);

    /* Call wrapper function. */
    Vector<llvm::Value *> call_inputs = interface.inputs();
    if (settings.maintain_stack()) {
      call_inputs.append(interface.context_ptr());
    }
    llvm::Value *output_struct = builder.CreateCall(wrapper_function, call_inputs);

    /* Extract output values. */
    for (uint i = 0; i < output_type_infos.size(); i++) {
      llvm::Value *out = builder.CreateExtractValue(output_struct, i);
      interface.set_output(i, out);
    }
  }

 private:
  llvm::Function *get_wrapper_function(CodeBuilder &builder,
                                       CodeInterface &interface,
                                       const BuildIRSettings &settings,
                                       ArrayRef<LLVMTypeInfo *> input_type_infos,
                                       ArrayRef<LLVMTypeInfo *> output_type_infos) const
  {
    Function *fn = m_tuple_call.owner();

    Vector<llvm::Type *> input_types = builder.types_of_values(interface.inputs());
    if (settings.maintain_stack()) {
      input_types.append(builder.getAnyPtrTy());
    }

    Vector<llvm::Type *> output_types;
    for (auto type_info : output_type_infos) {
      output_types.append(type_info->get_type(builder.getContext()));
    }

    llvm::Function *wrapper_function;
    FunctionIRCache &function_cache = interface.function_ir_cache();
    if (function_cache.contains((void *)this)) {
      wrapper_function = function_cache.lookup((void *)this);
    }
    else {
      llvm::Type *wrapper_output_type = builder.getStructType(output_types);

      llvm::FunctionType *wrapper_function_type = builder.getFunctionType(wrapper_output_type,
                                                                          input_types);

      wrapper_function = llvm::Function::Create(wrapper_function_type,
                                                llvm::GlobalValue::LinkageTypes::InternalLinkage,
                                                fn->name() + " Wrapper",
                                                builder.getModule());

      this->build_wrapper_function(
          settings, wrapper_function, input_type_infos, output_type_infos, wrapper_output_type);

      function_cache.add((void *)this, wrapper_function);
    }
    return wrapper_function;
  }

  void build_wrapper_function(const BuildIRSettings &settings,
                              llvm::Function *function,
                              ArrayRef<LLVMTypeInfo *> input_type_infos,
                              ArrayRef<LLVMTypeInfo *> output_type_infos,
                              llvm::Type *output_type) const
  {
    llvm::LLVMContext &context = function->getContext();
    llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
    CodeBuilder builder(bb);

    /* Allocate temporary stack buffer for tuple input and output. */
    auto &meta_in = m_tuple_call.meta_in();
    auto &meta_out = m_tuple_call.meta_out();
    llvm::Value *tuple_in_data_ptr = builder.CreateAllocaBytes_BytePtr(meta_in->size_of_data());
    tuple_in_data_ptr->setName("tuple_in_data");
    llvm::Value *tuple_out_data_ptr = builder.CreateAllocaBytes_BytePtr(meta_out->size_of_data());
    tuple_out_data_ptr->setName("tuple_out_data");

    /* Write input values into buffer. */
    for (uint i = 0; i < input_type_infos.size(); i++) {
      llvm::Value *arg = function->arg_begin() + i;
      llvm::Value *store_at_addr = builder.CreateConstGEP1_32(tuple_in_data_ptr,
                                                              meta_in->offsets()[i]);
      input_type_infos[i]->build_store_ir__relocate(builder, arg, store_at_addr);
    }

    /* Get execution stack for tuple call. */
    llvm::Value *context_ptr = nullptr;
    if (settings.maintain_stack()) {
      context_ptr = function->arg_begin() + input_type_infos.size();
    }
    else {
      context_ptr = build__stack_allocate_ExecutionContext(builder);
    }

    /* Execute tuple call body. */
    builder.CreateCallPointer(
        (void *)run_TupleCallBody,
        {builder.getAnyPtr(&m_tuple_call), tuple_in_data_ptr, tuple_out_data_ptr, context_ptr},
        builder.getVoidTy(),
        "Run tuple call body");

    /* Read output values from buffer. */
    llvm::Value *output = llvm::UndefValue::get(output_type);
    for (uint i = 0; i < output_type_infos.size(); i++) {
      llvm::Value *load_from_addr = builder.CreateConstGEP1_32(tuple_out_data_ptr,
                                                               meta_out->offsets()[i]);
      llvm::Value *out = output_type_infos[i]->build_load_ir__relocate(builder, load_from_addr);
      output = builder.CreateInsertValue(output, out, i);
    }

    builder.CreateRet(output);
  }
};

void derive_LLVMBuildIRBody_from_TupleCallBody(SharedFunction &fn)
{
  BLI_assert(fn->has_body<TupleCallBody>());
  BLI_assert(!fn->has_body<LLVMBuildIRBody>());

  fn->add_body<TupleCallLLVM>(fn->body<TupleCallBody>());
}

} /* namespace FN */
