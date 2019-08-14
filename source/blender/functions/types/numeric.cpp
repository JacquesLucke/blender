#include "numeric.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_cpp.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Types {

BLI_LAZY_INIT(Type *, GET_TYPE_float)
{
  Type *type = new Type("Float");
  type->add_extension<CPPTypeInfoForType<float>>();
  type->add_extension<PackedLLVMTypeInfo>(
      [](llvm::LLVMContext &context) { return llvm::Type::getFloatTy(context); });
  return type;
}

BLI_LAZY_INIT(Type *, GET_TYPE_int32)
{
  Type *type = new Type("Int32");
  type->add_extension<CPPTypeInfoForType<int32_t>>();
  type->add_extension<PackedLLVMTypeInfo>(
      [](llvm::LLVMContext &context) { return llvm::Type::getIntNTy(context, 32); });
  return type;
}

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

BLI_LAZY_INIT(Type *, GET_TYPE_float3)
{
  Type *type = new Type("Float3");
  type->add_extension<CPPTypeInfoForType<float3>>();
  type->add_extension<FloatVectorType>(3);
  return type;
}

BLI_LAZY_INIT(Type *, GET_TYPE_rgba_f)
{
  Type *type = new Type("RGBA Float");
  type->add_extension<CPPTypeInfoForType<rgba_f>>();
  type->add_extension<FloatVectorType>(4);
  return type;
}

}  // namespace Types
}  // namespace FN
