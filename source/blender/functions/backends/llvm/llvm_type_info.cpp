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

/******************** PointerLLVMTypeInfo ********************/

llvm::Type *PointerLLVMTypeInfo::get_type(llvm::LLVMContext &context) const
{
  return llvm::Type::getInt8PtrTy(context);
}

void *PointerLLVMTypeInfo::copy_value(PointerLLVMTypeInfo *info, void *value)
{
  return info->m_copy_func(value);
}

void PointerLLVMTypeInfo::free_value(PointerLLVMTypeInfo *info, void *value)
{
  info->m_free_func(value);
}

void *PointerLLVMTypeInfo::default_value(PointerLLVMTypeInfo *info)
{
  return info->m_default_func();
}

llvm::Value *PointerLLVMTypeInfo::build_copy_ir(CodeBuilder &builder, llvm::Value *value) const
{
  auto *any_ptr_ty = builder.getAnyPtrTy();
  auto *copy_ftype = llvm::FunctionType::get(any_ptr_ty, {any_ptr_ty, any_ptr_ty}, false);

  return builder.CreateCallPointer((void *)PointerLLVMTypeInfo::copy_value,
                                   copy_ftype,
                                   {builder.getAnyPtr(this), value},
                                   "Copy value");
}

void PointerLLVMTypeInfo::build_free_ir(CodeBuilder &builder, llvm::Value *value) const
{
  builder.CreateCallPointer((void *)PointerLLVMTypeInfo::free_value,
                            {builder.getAnyPtr(this), value},
                            builder.getVoidTy(),
                            "Free value");
}

void PointerLLVMTypeInfo::build_store_ir__copy(CodeBuilder &builder,
                                               llvm::Value *value,
                                               llvm::Value *address) const
{
  auto *copied_value = this->build_copy_ir(builder, value);
  this->build_store_ir__relocate(builder, copied_value, address);
}

void PointerLLVMTypeInfo::build_store_ir__relocate(CodeBuilder &builder,
                                                   llvm::Value *value,
                                                   llvm::Value *address) const
{
  auto *addr = builder.CastToPointerOf(address, builder.getAnyPtrTy());
  builder.CreateStore(value, addr);
}

llvm::Value *PointerLLVMTypeInfo::build_load_ir__copy(CodeBuilder &builder,
                                                      llvm::Value *address) const
{
  auto *value = this->build_load_ir__relocate(builder, address);
  this->build_copy_ir(builder, value);
  return value;
}

llvm::Value *PointerLLVMTypeInfo::build_load_ir__relocate(CodeBuilder &builder,
                                                          llvm::Value *address) const
{
  auto *addr = builder.CastToPointerOf(address, builder.getAnyPtrTy());
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

};  // namespace FN
