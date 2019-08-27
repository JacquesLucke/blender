#include "BLI_lazy_init.hpp"

#include "FN_types.hpp"
#include "FN_cpp.hpp"
#include "FN_llvm.hpp"
#include "DNA_object_types.h"

namespace FN {
namespace Types {

Type *TYPE_string = nullptr;
Type *TYPE_string_list = nullptr;

class LLVMString : public LLVMTypeInfo {
 public:
  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::Type::getInt8PtrTy(context);
  }

  static char *copy_string(char *str)
  {
    if (str == nullptr) {
      return nullptr;
    }
    return (char *)MEM_dupallocN(str);
  }

  static void free_string(char *str)
  {
    if (str != nullptr) {
      MEM_freeN(str);
    }
  }

  static void store_relocate(char *str, void *address)
  {
    new (address) MyString(str);
    free_string(str);
  }

  static void store_copy(char *str, void *address)
  {
    new (address) MyString(str);
  }

  llvm::Value *build_copy_ir(CodeBuilder &builder, llvm::Value *value) const override
  {
    return builder.CreateCallPointer(
        (void *)copy_string, {value}, builder.getInt8PtrTy(), "copy string");
  }

  void build_free_ir(CodeBuilder &builder, llvm::Value *value) const override
  {
    builder.CreateCallPointer((void *)free_string, {value}, builder.getVoidTy(), "free string");
  }

  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *address) const override
  {
    builder.CreateCallPointer(
        (void *)store_relocate, {value, address}, builder.getVoidTy(), "store string relocate");
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *address) const override
  {
    builder.CreateCallPointer(
        (void *)store_copy, {value, address}, builder.getVoidTy(), "store string copy");
  }

  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *address) const override
  {
    llvm::Value *data_address = builder.CastToPointerOf(address, builder.getInt8PtrTy());
    llvm::Value *str = builder.CreateLoad(data_address);
    builder.CreateStore(builder.getInt8Ptr(nullptr), data_address);
    return str;
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override
  {
    llvm::Value *data_address = builder.CastToPointerOf(address, builder.getInt8PtrTy());
    llvm::Value *str = builder.CreateLoad(data_address);
    llvm::Value *str_copy = this->build_copy_ir(builder, str);
    return str_copy;
  }
};

void INIT_string(Vector<Type *> &types_to_free)
{
  TYPE_string = new Type("String");
  TYPE_string->add_extension<CPPTypeInfoForType<MyString>>();
  TYPE_string->add_extension<LLVMString>();

  TYPE_string_list = new_list_type(TYPE_string);

  types_to_free.extend({TYPE_string, TYPE_string_list});
}

}  // namespace Types
}  // namespace FN
