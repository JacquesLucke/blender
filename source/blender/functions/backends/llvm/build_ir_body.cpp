#include "FN_llvm.hpp"

namespace FN {

llvm::Function *LLVMBuildIRBody::build_function(llvm::Module *module,
                                                StringRef name,
                                                BuildIRSettings &settings,
                                                FunctionIRCache &function_cache)
{
  Function *owner_fn = this->owner();
  llvm::LLVMContext &context = module->getContext();

  uint input_amount = owner_fn->input_amount();
  uint output_amount = owner_fn->output_amount();

  Vector<LLVMTypeInfo *> input_type_infos(input_amount);
  Vector<LLVMTypeInfo *> output_type_infos(output_amount);
  Vector<llvm::Type *> input_types(input_amount);
  Vector<llvm::Type *> output_types(output_amount);

  for (uint i = 0; i < input_amount; i++) {
    LLVMTypeInfo &type_info = owner_fn->input_type(i)->extension<LLVMTypeInfo>();
    input_type_infos[i] = &type_info;
    input_types[i] = type_info.get_type(context);
  }

  for (uint i = 0; i < output_amount; i++) {
    LLVMTypeInfo &type_info = owner_fn->output_type(i)->extension<LLVMTypeInfo>();
    output_type_infos[i] = &type_info;
    output_types[i] = type_info.get_type(context);
  }

  Vector<llvm::Type *> arg_types = input_types;
  arg_types.append(llvm::Type::getInt8PtrTy(context));
  llvm::Type *return_type = llvm::StructType::get(context, to_llvm(output_types));

  llvm::FunctionType *ftype = llvm::FunctionType::get(return_type, to_llvm(arg_types), false);
  llvm::Function *function = llvm::Function::Create(
      ftype, llvm::GlobalValue::LinkageTypes::ExternalLinkage, to_llvm(name), module);
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
  CodeBuilder builder(bb);

  Vector<llvm::Value *> input_values(input_amount);
  for (uint i = 0; i < input_amount; i++) {
    input_values[i] = builder.take_function_input(i, owner_fn->input_name(i));
  }
  llvm::Value *context_ptr = builder.take_function_input(input_amount, "context_ptr");
  Vector<llvm::Value *> output_values(output_amount);

  CodeInterface interface(input_values, output_values, context_ptr, function_cache);
  this->build_ir(builder, interface, settings);

  llvm::Value *output_value_struct = builder.getUndef(return_type);
  for (uint i = 0; i < output_amount; i++) {
    output_value_struct = builder.CreateInsertValue(output_value_struct, output_values[i], i);
  }

  builder.CreateRet(output_value_struct);
  return function;
}

} /* namespace FN */
