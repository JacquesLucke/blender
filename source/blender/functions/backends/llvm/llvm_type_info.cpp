#include "llvm_type_info.hpp"

namespace FN {

/******************** LLVMTypeInfo ********************/

LLVMTypeInfo::~LLVMTypeInfo()
{
}

/******************** TrivialLLVMTypeInfo ********************/

llvm::Value *TrivialLLVMTypeInfo::build_copy_ir(CodeBuilder &UNUSED(builder),
                                                llvm::Value *value) const
{
  return value;
}

void TrivialLLVMTypeInfo::build_free_ir(CodeBuilder &UNUSED(builder),
                                        llvm::Value *UNUSED(value)) const
{
  return;
}

llvm::Value *TrivialLLVMTypeInfo::build_load_ir__relocate(CodeBuilder &builder,
                                                          llvm::Value *address) const
{
  return this->build_load_ir__copy(builder, address);
}

void TrivialLLVMTypeInfo::build_store_ir__relocate(CodeBuilder &builder,
                                                   llvm::Value *value,
                                                   llvm::Value *address) const
{
  return this->build_store_ir__copy(builder, value, address);
}

/******************** PackedLLVMTypeInfo ********************/

llvm::Type *PackedLLVMTypeInfo::get_type(llvm::LLVMContext &context) const
{
  return m_create_func(context);
}

void PackedLLVMTypeInfo::build_store_ir__copy(CodeBuilder &builder,
                                              llvm::Value *value,
                                              llvm::Value *address) const
{
  llvm::Type *type = value->getType();
  llvm::Value *addr = builder.CastToPointerOf(address, type);
  builder.CreateStore(value, addr);
}

llvm::Value *PackedLLVMTypeInfo::build_load_ir__copy(CodeBuilder &builder,
                                                     llvm::Value *address) const
{
  llvm::Type *type = this->get_type(builder.getContext());
  llvm::Value *addr = builder.CastToPointerOf(address, type);
  return builder.CreateLoad(addr);
}

/* Utilities
 ******************************************/

Vector<llvm::Type *> types_of_type_infos(const Vector<LLVMTypeInfo *> &type_infos,
                                         llvm::LLVMContext &context)
{
  Vector<llvm::Type *> types;
  for (auto info : type_infos) {
    types.append(info->get_type(context));
  }
  return types;
}

}  // namespace FN
