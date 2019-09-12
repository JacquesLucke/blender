#include "BLI_lazy_init_cxx.h"

#include "FN_types.hpp"
#include "FN_cpp.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Types {

class FloatVectorType : public TrivialLLVMTypeInfo {
 private:
  uint m_size;

 public:
  FloatVectorType(uint size) : m_size(size)
  {
  }

  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::VectorType::get(llvm::Type::getFloatTy(context), m_size);
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *vector,
                            llvm::Value *address) const override
  {
    address = builder.CastToPointerOf(address, builder.getFloatTy());
    for (uint i = 0; i < m_size; i++) {
      llvm::Value *value = builder.CreateExtractElement(vector, i);
      llvm::Value *value_address = builder.CreateConstGEP1_32(address, i);
      builder.CreateStore(value, value_address);
    }
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override
  {
    llvm::Value *vector = builder.getUndef(this->get_type(builder.getContext()));
    address = builder.CastToPointerOf(address, builder.getFloatTy());
    for (uint i = 0; i < m_size; i++) {
      llvm::Value *value_address = builder.CreateConstGEP1_32(address, i);
      llvm::Value *value = builder.CreateLoad(value_address);
      vector = builder.CreateInsertElement(vector, value, i);
    }
    return vector;
  }
};

Type *TYPE_float = nullptr;
Type *TYPE_int32 = nullptr;
Type *TYPE_float3 = nullptr;
Type *TYPE_rgba_f = nullptr;

Type *TYPE_float_list = nullptr;
Type *TYPE_int32_list = nullptr;
Type *TYPE_float3_list = nullptr;
Type *TYPE_rgba_f_list = nullptr;

void INIT_numeric(Vector<Type *> &types_to_free)
{
  TYPE_float = new Type("Float");
  TYPE_float->add_extension<CPPTypeInfoForType<float>>();
  TYPE_float->add_extension<PackedLLVMTypeInfo>(
      [](llvm::LLVMContext &context) { return llvm::Type::getFloatTy(context); });

  TYPE_int32 = new Type("Int32");
  TYPE_int32->add_extension<CPPTypeInfoForType<int32_t>>();
  TYPE_int32->add_extension<PackedLLVMTypeInfo>(
      [](llvm::LLVMContext &context) { return llvm::Type::getIntNTy(context, 32); });

  TYPE_float3 = new Type("Float3");
  TYPE_float3->add_extension<CPPTypeInfoForType<float3>>();
  TYPE_float3->add_extension<FloatVectorType>(3);

  TYPE_rgba_f = new Type("RGBA Float");
  TYPE_rgba_f->add_extension<CPPTypeInfoForType<rgba_f>>();
  TYPE_rgba_f->add_extension<FloatVectorType>(4);

  TYPE_float_list = new_list_type(TYPE_float);
  TYPE_int32_list = new_list_type(TYPE_int32);
  TYPE_float3_list = new_list_type(TYPE_float3);
  TYPE_rgba_f_list = new_list_type(TYPE_rgba_f);

  types_to_free.extend({
      TYPE_float,
      TYPE_int32,
      TYPE_float3,
      TYPE_rgba_f,
      TYPE_float_list,
      TYPE_int32_list,
      TYPE_float3_list,
      TYPE_rgba_f_list,
  });
}

}  // namespace Types
}  // namespace FN
