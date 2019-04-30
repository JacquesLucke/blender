#include "boolean.hpp"

#include "BLI_lazy_init.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Types {

class LLVMBool : public LLVMTypeInfo {

  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::Type::getInt1Ty(context);
  }

  llvm::Value *build_copy_ir(CodeBuilder &UNUSED(builder), llvm::Value *value) const override
  {
    return value;
  }

  void build_free_ir(CodeBuilder &UNUSED(builder), llvm::Value *UNUSED(value)) const override
  {
    return;
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *byte_addr) const override
  {
    llvm::Value *byte_value = builder.CreateCastIntTo8(value, false);
    builder.CreateStore(byte_value, byte_addr);
  }

  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *byte_addr) const override
  {
    this->build_store_ir__copy(builder, value, byte_addr);
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *byte_addr) const override
  {
    llvm::Value *byte_value = builder.CreateLoad(byte_addr);
    llvm::Value *value = builder.CreateCastIntTo1(byte_value);
    return value;
  }

  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *byte_addr) const override
  {
    return this->build_load_ir__copy(builder, byte_addr);
  }
};

LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_bool)
{
  SharedType type = SharedType::New("Bool");
  type->extend(new CPPTypeInfoForType<bool>());
  type->extend(new LLVMBool());
  return type;
}

}  // namespace Types
}  // namespace FN
