#include "boolean.hpp"

#include "BLI_lazy_init.hpp"

#include "FN_cpp.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Types {

class LLVMBool : public TrivialLLVMTypeInfo {

  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::Type::getInt1Ty(context);
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *byte_addr) const override
  {
    llvm::Value *byte_value = builder.CreateCastIntTo8(value, false);
    builder.CreateStore(byte_value, byte_addr);
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *byte_addr) const override
  {
    llvm::Value *byte_value = builder.CreateLoad(byte_addr);
    llvm::Value *value = builder.CreateCastIntTo1(byte_value);
    return value;
  }
};

BLI_LAZY_INIT(SharedType, GET_TYPE_bool)
{
  SharedType type = SharedType::New("Bool");
  type->add_extension<CPPTypeInfoForType<bool>>();
  type->add_extension<LLVMBool>();
  return type;
}

}  // namespace Types
}  // namespace FN
