#pragma once

/**
 * This is a wrapper for llvm::IRBuilder<>. It allows extending the normal builder with methods
 * that are helpful for the use cases in Blender.
 */

#include "FN_core.hpp"
#include <llvm/IR/IRBuilder.h>

namespace FN {

class LLVMTypeInfo;
class IRConstruct_ForLoop;
class IRConstruct_IterationsLoop;
class IRConstruct_IfThenElse;

template<typename T> static llvm::ArrayRef<T> to_llvm(const Vector<T> &vector)
{
  return llvm::ArrayRef<T>(vector.begin(), vector.end());
}

template<typename T> static llvm::ArrayRef<T> to_llvm(ArrayRef<T> array_ref)
{
  return llvm::ArrayRef<T>(array_ref.begin(), array_ref.end());
}

inline llvm::StringRef to_llvm(StringRef string)
{
  return llvm::StringRef(string.begin(), string.size());
}

class CodeBuilder {
 private:
  llvm::IRBuilder<> m_builder;

 public:
  CodeBuilder(llvm::IRBuilder<> &builder) : m_builder(builder)
  {
  }

  CodeBuilder(llvm::BasicBlock *bb) : m_builder(bb)
  {
  }

  /* Getters
   ***************************************/

  llvm::LLVMContext &getContext()
  {
    return m_builder.getContext();
  }

  llvm::Module *getModule()
  {
    return m_builder.GetInsertBlock()->getModule();
  }

  llvm::BasicBlock *GetInsertBlock()
  {
    return m_builder.GetInsertBlock();
  }

  llvm::Function *GetFunction()
  {
    return this->GetInsertBlock()->getParent();
  }

  llvm::Type *getFloatTy()
  {
    return m_builder.getFloatTy();
  }

  llvm::Type *getDoubleTy()
  {
    return m_builder.getDoubleTy();
  }

  llvm::Type *getVoidTy()
  {
    return m_builder.getVoidTy();
  }

  /**
   * LLVM does not permit void*, so use int8* instead.
   * https://llvm.org/docs/LangRef.html#pointer-type
   */
  llvm::Type *getAnyPtrTy()
  {
    return this->getInt8PtrTy();
  }

  llvm::Type *getInt8Ty()
  {
    return m_builder.getInt8Ty();
  }

  llvm::Type *getInt8PtrTy()
  {
    return m_builder.getInt8PtrTy();
  }

  llvm::Type *getInt32Ty()
  {
    return m_builder.getInt32Ty();
  }

  llvm::Type *getFixedSizeType(uint size)
  {
    return llvm::ArrayType::get(m_builder.getInt8Ty(), size);
  }

  llvm::Type *getStructType(ArrayRef<llvm::Type *> types)
  {
    return llvm::StructType::get(this->getContext(), to_llvm(types));
  }

  llvm::FunctionType *getFunctionType(llvm::Type *ret_type, ArrayRef<llvm::Type *> arg_types)
  {
    return llvm::FunctionType::get(ret_type, to_llvm(arg_types), false);
  }

  /* Value Builders
   **************************************/

  llvm::UndefValue *getUndef(llvm::Type *type)
  {
    return llvm::UndefValue::get(type);
  }

  template<typename T> llvm::Value *getAnyPtr(T *ptr)
  {
    return this->getPtr((void *)ptr, this->getAnyPtrTy());
  }

  llvm::Value *getPtr(void *ptr, llvm::Type *ptr_type)
  {
    llvm::Value *ptr_value = m_builder.getInt64((size_t)ptr);
    return m_builder.CreateIntToPtr(ptr_value, ptr_type);
  }

  llvm::ConstantInt *getInt64(int64_t value)
  {
    return m_builder.getInt64(value);
  }

  llvm::ConstantInt *getInt32(int value)
  {
    return m_builder.getInt32(value);
  }

  llvm::ConstantInt *getInt1(bool value)
  {
    return m_builder.getInt1(value);
  }

  llvm::Value *getInt8Ptr(const char *ptr)
  {
    return this->getPtr((void *)ptr, this->getInt8PtrTy());
  }

  llvm::Constant *getFloat(float value)
  {
    return llvm::ConstantFP::get(this->getFloatTy(), value);
  }

  llvm::Constant *getDouble(double value)
  {
    return llvm::ConstantFP::get(this->getDoubleTy(), value);
  }

  /* Create new blocks
   **************************************/

  llvm::BasicBlock *NewBlockInFunction(StringRef name)
  {
    auto *new_block = llvm::BasicBlock::Create(
        this->getContext(), name.data(), this->GetFunction());
    return new_block;
  }

  CodeBuilder NewBuilderInNewBlock(StringRef name)
  {
    return CodeBuilder(this->NewBlockInFunction(name.data()));
  }

  /* Misc
   **************************************/

  void SetInsertPoint(llvm::BasicBlock *block)
  {
    m_builder.SetInsertPoint(block);
  }

  Vector<llvm::Type *> types_of_values(ArrayRef<llvm::Value *> values);

  llvm::Value *take_function_input(uint index, StringRef name)
  {
    llvm::Value *value = this->GetFunction()->arg_begin() + index;
    value->setName(to_llvm(name));
    return value;
  }

  /* Instruction Builders
   **************************************/

  void CreateRet(llvm::Value *value)
  {
    m_builder.CreateRet(value);
  }

  void CreateRetVoid()
  {
    m_builder.CreateRetVoid();
  }

  llvm::PHINode *CreatePhi(llvm::Type *type, uint reserved_values)
  {
    return m_builder.CreatePHI(type, reserved_values);
  }

  llvm::Value *CreateICmpULT(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateICmpULT(a, b);
  }

  llvm::Value *CreateICmpEQ(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateICmpEQ(a, b);
  }

  llvm::Value *CreateFCmpOLT(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateFCmpOLT(a, b);
  }

  void CreateBr(llvm::BasicBlock *destination_block)
  {
    m_builder.CreateBr(destination_block);
  }

  void CreateCondBr(llvm::Value *condition,
                    llvm::BasicBlock *true_block,
                    llvm::BasicBlock *false_block)
  {
    m_builder.CreateCondBr(condition, true_block, false_block);
  }

  llvm::Value *CreateCastIntTo8(llvm::Value *value, bool is_signed)
  {
    return m_builder.CreateIntCast(value, m_builder.getInt8Ty(), is_signed);
  }

  llvm::Value *CreateCastIntTo1(llvm::Value *value)
  {
    return m_builder.CreateIntCast(value, m_builder.getInt1Ty(), false);
  }

  llvm::Value *CreateIAdd(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateAdd(a, b);
  }

  llvm::Value *CreateIAdd(llvm::Value *a, int value)
  {
    return m_builder.CreateAdd(a, this->getInt32(value));
  }

  llvm::Value *CreateFAdd(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateFAdd(a, b);
  }

  llvm::Value *CreateFMul(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateFMul(a, b);
  }

  llvm::Value *CreateURem(llvm::Value *a, llvm::Value *b)
  {
    return m_builder.CreateURem(a, b);
  }

  llvm::Value *CreateAllocaBytes_AnyPtr(uint amount)
  {
    llvm::Type *size_type = this->getFixedSizeType(amount);
    llvm::Value *addr = m_builder.CreateAlloca(size_type);
    return this->CastToAnyPtr(addr);
  }

  llvm::Value *CreateAllocaBytes_BytePtr(uint amount)
  {
    llvm::Type *size_type = this->getFixedSizeType(amount);
    llvm::Value *addr = m_builder.CreateAlloca(size_type);
    return this->CastToBytePtr(addr);
  }

  llvm::Value *CreateIntToPtr(llvm::Value *value, llvm::Type *pointer_type)
  {
    return m_builder.CreateIntToPtr(value, pointer_type);
  }

  llvm::Value *CastToPointerOf(llvm::Value *addr, llvm::Type *base_type)
  {
    return m_builder.CreatePointerCast(addr, base_type->getPointerTo());
  }

  llvm::Value *CastToPointerWithStride(llvm::Value *addr, uint stride)
  {
    llvm::Type *stride_type = this->getFixedSizeType(stride);
    return this->CastToPointerOf(addr, stride_type);
  }

  llvm::Value *CastToAnyPtr(llvm::Value *addr)
  {
    return m_builder.CreatePointerCast(addr, this->getAnyPtrTy());
  }

  llvm::Value *CastToBytePtr(llvm::Value *addr)
  {
    return m_builder.CreatePointerCast(addr, m_builder.getInt8PtrTy());
  }

  llvm::Value *CastFloatToDouble(llvm::Value *value)
  {
    return m_builder.CreateFPCast(value, this->getDoubleTy());
  }

  llvm::Value *CreateLoad(llvm::Value *addr)
  {
    return m_builder.CreateLoad(addr);
  }

  llvm::Value *CreateLoadAtIndex(llvm::Value *array_start_addr, uint index)
  {
    llvm::Value *addr = this->CreateConstGEP1_32(array_start_addr, index);
    llvm::Value *value = this->CreateLoad(addr);
    return value;
  }

  llvm::Value *CreateLoadAtIndex(llvm::Value *array_start_addr, llvm::Value *index)
  {
    llvm::Value *addr = this->CreateGEP(array_start_addr, index);
    llvm::Value *value = this->CreateLoad(addr);
    return value;
  }

  void CreateStore(llvm::Value *value, llvm::Value *addr)
  {
    m_builder.CreateStore(value, addr, false);
  }

  llvm::Value *CreateExtractValue(llvm::Value *aggregate, uint index)
  {
    BLI_assert(aggregate->getType()->isStructTy());
    return m_builder.CreateExtractValue(aggregate, index);
  }

  llvm::Value *CreateInsertValue(llvm::Value *aggregate, llvm::Value *value, uint index)
  {
    BLI_assert(aggregate->getType()->isStructTy());
    return m_builder.CreateInsertValue(aggregate, value, index);
  }

  llvm::Value *CreateExtractElement(llvm::Value *vector, uint index)
  {
    BLI_assert(vector->getType()->isVectorTy());
    return m_builder.CreateExtractElement(vector, index);
  }

  llvm::Value *CreateInsertElement(llvm::Value *vector, llvm::Value *value, uint index)
  {
    BLI_assert(vector->getType()->isVectorTy());
    return m_builder.CreateInsertElement(vector, value, index);
  }

  llvm::Value *CreateCallPointer(void *func_ptr,
                                 llvm::FunctionType *ftype,
                                 ArrayRef<llvm::Value *> args,
                                 const char *function_name = "");
  llvm::Value *CreateCallPointer(void *func_ptr,
                                 ArrayRef<llvm::Value *> args,
                                 llvm::Type *return_type,
                                 const char *function_name = "");

  llvm::Value *CreateCall(llvm::Function *function, ArrayRef<llvm::Value *> args)
  {
    return m_builder.CreateCall(function, to_llvm(args));
  }

  llvm::Value *CreateConstGEP1_32(llvm::Value *addr, uint index)
  {
    return m_builder.CreateConstGEP1_32(addr, index);
  }

  llvm::Value *CreateGEP(llvm::Value *addr, llvm::Value *index)
  {
    return m_builder.CreateGEP(addr, index);
  }

  llvm::Value *CreateSin(llvm::Value *value)
  {
    auto *function = llvm::Intrinsic::getDeclaration(
        this->getModule(), llvm::Intrinsic::sin, value->getType());
    return m_builder.CreateCall(function, value);
  }

  llvm::Value *CreateSIntMax(llvm::Value *a, llvm::Value *b)
  {
    llvm::Value *a_is_larger = m_builder.CreateICmpSGE(a, b);
    return m_builder.CreateSelect(a_is_larger, a, b);
  }

  llvm::Value *CreateSIntMax(ArrayRef<llvm::Value *> values)
  {
    BLI_assert(values.size() >= 1);
    llvm::Value *max_value = values[0];
    for (llvm::Value *value : values.drop_front(1)) {
      max_value = this->CreateSIntMax(max_value, value);
    }
    return max_value;
  }

  llvm::SwitchInst *CreateSwitch(llvm::Value *value,
                                 llvm::BasicBlock *default_destination,
                                 uint case_amount)
  {
    return m_builder.CreateSwitch(value, default_destination, case_amount);
  }

  llvm::Value *CreateStructToVector(llvm::Value *value)
  {
    llvm::Type *struct_type = value->getType();
    BLI_assert(struct_type->isStructTy());
    uint length = struct_type->getStructNumElements();

    llvm::Type *base_type = struct_type->getStructElementType(0);
    llvm::Type *vector_type = llvm::VectorType::get(base_type, length);

    llvm::Value *output = llvm::UndefValue::get(vector_type);
    for (uint i = 0; i < length; i++) {
      output = m_builder.CreateInsertElement(output, m_builder.CreateExtractValue(value, i), i);
    }
    return output;
  }

  llvm::Value *CreateVectorToStruct(llvm::Value *value)
  {
    llvm::Type *vector_type = value->getType();
    BLI_assert(vector_type->isVectorTy());
    uint length = vector_type->getVectorNumElements();

    llvm::Type *base_type = vector_type->getVectorElementType();
    Vector<llvm::Type *> types(length);
    types.fill(base_type);
    llvm::Type *struct_type = this->getStructType(types);

    llvm::Value *output = llvm::UndefValue::get(struct_type);
    for (uint i = 0; i < length; i++) {
      output = m_builder.CreateInsertValue(output, m_builder.CreateExtractElement(value, i), i);
    }
    return output;
  }

  void CreateAssert(llvm::Value *condition, const char *message = "");
  void CreateAssertFalse(const char *message = "");

  /* Print
   **************************************/

  void CreatePrintf(const char *format, ArrayRef<llvm::Value *> values = {});
  void CreatePrintfWithStacktrace(llvm::Value *context_ptr,
                                  const char *format,
                                  ArrayRef<llvm::Value *> values);

  /* Control Flow Construction
   **************************************/

  IRConstruct_ForLoop CreateForLoop(StringRef name = "");
  IRConstruct_IterationsLoop CreateNIterationsLoop(llvm::Value *iterations, StringRef name = "");
  IRConstruct_IfThenElse CreateIfThenElse(llvm::Value *condition, StringRef name = "");
};

class IRConstruct_ForLoop {
 private:
  CodeBuilder m_entry;
  CodeBuilder m_condition;
  CodeBuilder m_body;

  llvm::BasicBlock *m_condition_entry;
  llvm::BasicBlock *m_body_entry;

 public:
  IRConstruct_ForLoop(CodeBuilder entry, CodeBuilder condition, CodeBuilder body)
      : m_entry(entry),
        m_condition(condition),
        m_body(body),
        m_condition_entry(condition.GetInsertBlock()),
        m_body_entry(body.GetInsertBlock())
  {
  }

  CodeBuilder &entry_builder()
  {
    return m_entry;
  }

  CodeBuilder &condition_builder()
  {
    return m_condition;
  }

  CodeBuilder &body_builder()
  {
    return m_body;
  }

  void finalize(CodeBuilder &after_builder, llvm::Value *condition);
};

class IRConstruct_IterationsLoop {
 private:
  IRConstruct_ForLoop m_loop;
  llvm::Value *m_iterations;
  llvm::PHINode *m_current_iteration;

 public:
  IRConstruct_IterationsLoop(IRConstruct_ForLoop loop,
                             llvm::Value *iterations,
                             llvm::PHINode *current_iteration)
      : m_loop(loop), m_iterations(iterations), m_current_iteration(current_iteration)
  {
  }

  CodeBuilder &body_builder()
  {
    return m_loop.body_builder();
  }

  llvm::Value *current_iteration()
  {
    return m_current_iteration;
  }

  void finalize(CodeBuilder &after_builder);
};

class IRConstruct_IfThenElse {
 private:
  CodeBuilder m_then_builder;
  CodeBuilder m_else_builder;

 public:
  IRConstruct_IfThenElse(CodeBuilder then_builder, CodeBuilder else_builder)
      : m_then_builder(then_builder), m_else_builder(else_builder)
  {
  }

  CodeBuilder &then_builder()
  {
    return m_then_builder;
  }

  CodeBuilder &else_builder()
  {
    return m_else_builder;
  }

  void finalize(CodeBuilder &after_builder);
};

} /* namespace FN */
