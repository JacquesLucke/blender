#include "BLI_lazy_init.hpp"

#include "FN_types.hpp"
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
                            llvm::Value *address) const override
  {
    llvm::Value *byte_address = builder.CastToPointerOf(address, builder.getInt8Ty());
    llvm::Value *byte_value = builder.CreateCastIntTo8(value, false);
    builder.CreateStore(byte_value, byte_address);
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override
  {
    llvm::Value *byte_address = builder.CastToPointerOf(address, builder.getInt8Ty());
    llvm::Value *byte_value = builder.CreateLoad(byte_address);
    llvm::Value *value = builder.CreateCastIntTo1(byte_value);
    return value;
  }
};

Type *TYPE_bool = nullptr;
Type *TYPE_bool_list = nullptr;

void INIT_bool(Vector<Type *> &types_to_free)
{
  TYPE_bool = new Type("Bool");
  TYPE_bool->add_extension<CPPTypeInfoForType<bool>>();
  TYPE_bool->add_extension<LLVMBool>();

  TYPE_bool_list = new_list_type(TYPE_bool);

  types_to_free.extend({TYPE_bool, TYPE_bool_list});
}

}  // namespace Types
}  // namespace FN
