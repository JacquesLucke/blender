#include "FN_llvm.hpp"
#include "FN_tuple_call.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include "context_pool.hpp"

namespace FN {

typedef std::function<void(
    CodeBuilder &builder, CodeInterface &interface, const BuildIRSettings &settings)>
    BuildIRFunction;

static llvm::Value *lookup_tuple_address(CodeBuilder &builder,
                                         llvm::Value *data_addr,
                                         llvm::Value *offsets_addr,
                                         uint index)
{
  llvm::Value *offset_addr = builder.CreateConstGEP1_32(offsets_addr, index);
  llvm::Value *offset = builder.CreateLoad(offset_addr);
  llvm::Value *value_byte_addr = builder.CreateGEP(data_addr, offset);
  return value_byte_addr;
}

static llvm::Function *insert_tuple_call_function(SharedFunction &fn, llvm::Module *module)
{
  llvm::LLVMContext &context = module->getContext();
  LLVMBuildIRBody &body = fn->body<LLVMBuildIRBody>();

  llvm::Type *void_ty = llvm::Type::getVoidTy(context);
  llvm::Type *any_ptr_ty = llvm::Type::getInt8PtrTy(context);
  llvm::Type *byte_ptr_ty = llvm::Type::getInt8PtrTy(context);
  llvm::Type *int_ptr_ty = llvm::Type::getInt32PtrTy(context);

  Vector<llvm::Type *> input_types = {
      byte_ptr_ty,
      int_ptr_ty,
      byte_ptr_ty,
      int_ptr_ty,
      any_ptr_ty,
  };

  llvm::FunctionType *function_type = llvm::FunctionType::get(
      void_ty, to_llvm_array_ref(input_types), false);

  llvm::Function *function = llvm::Function::Create(
      function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage, fn->name().data(), module);

  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
  CodeBuilder builder(bb);

  llvm::Value *fn_in_data = function->arg_begin() + 0;
  llvm::Value *fn_in_offsets = function->arg_begin() + 1;
  llvm::Value *fn_out_data = function->arg_begin() + 2;
  llvm::Value *fn_out_offsets = function->arg_begin() + 3;
  llvm::Value *context_ptr = function->arg_begin() + 4;
  fn_in_data->setName("data_in");
  fn_in_offsets->setName("offsets_in");
  fn_out_data->setName("data_out");
  fn_out_offsets->setName("offsets_out");
  context_ptr->setName("context");

  Vector<llvm::Value *> input_values;
  for (uint i = 0; i < fn->input_amount(); i++) {
    llvm::Value *value_byte_addr = lookup_tuple_address(builder, fn_in_data, fn_in_offsets, i);

    LLVMTypeInfo &type_info = fn->input_type(i)->extension<LLVMTypeInfo>();
    llvm::Value *value = type_info.build_load_ir__relocate(builder, value_byte_addr);

    input_values.append(value);
  }

  Vector<llvm::Value *> output_values(fn->output_amount());
  BuildIRSettings settings;
  FunctionIRCache function_cache;
  CodeInterface interface(input_values, output_values, context_ptr, function_cache);
  body.build_ir(builder, interface, settings);

  for (uint i = 0; i < output_values.size(); i++) {
    llvm::Value *value_byte_addr = lookup_tuple_address(builder, fn_out_data, fn_out_offsets, i);

    LLVMTypeInfo &type_info = fn->output_type(i)->extension<LLVMTypeInfo>();
    type_info.build_store_ir__relocate(builder, output_values[i], value_byte_addr);
  }

  builder.CreateRetVoid();

  return function;
}

typedef void (*LLVMCallFN)(void *data_in,
                           const uint *offsets_in,
                           void *data_out,
                           const uint *offsets_out,
                           ExecutionContext *ctx);

class LLVMTupleCall : public TupleCallBody {
 private:
  std::unique_ptr<CompiledLLVM> m_compiled;
  LLVMCallFN m_call;

 public:
  LLVMTupleCall(std::unique_ptr<CompiledLLVM> compiled) : m_compiled(std::move(compiled))
  {
    m_call = (LLVMCallFN)m_compiled->function_ptr();
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const override
  {
    fn_out.destruct_all();
    BLI_assert(fn_in.all_initialized());
    BLI_assert(fn_out.all_uninitialized());

    m_call(fn_in.data_ptr(), fn_in.offsets_ptr(), fn_out.data_ptr(), fn_out.offsets_ptr(), &ctx);

    fn_in.set_all_uninitialized();
    fn_out.set_all_initialized();
  }
};

static std::unique_ptr<CompiledLLVM> compile_ir_to_tuple_call(SharedFunction &fn,
                                                              llvm::LLVMContext &context)
{
  llvm::Module *module = new llvm::Module(fn->name().data(), context);
  llvm::Function *function = insert_tuple_call_function(fn, module);

  auto compiled = CompiledLLVM::FromIR(module, function);
  return compiled;
}

void derive_TupleCallBody_from_LLVMBuildIRBody(SharedFunction &fn)
{
  BLI_assert(fn->has_body<LLVMBuildIRBody>());
  BLI_assert(!fn->has_body<TupleCallBody>());

  auto *context = aquire_llvm_context();
  fn->add_body<LLVMTupleCall>(compile_ir_to_tuple_call(fn, *context));
  release_llvm_context(context);
}

} /* namespace FN */
