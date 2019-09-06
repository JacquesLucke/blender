#pragma once

#include "FN_core.hpp"

#include "builder.hpp"

#include <functional>
#include <mutex>

namespace FN {

/**
 * This is the main type extension for llvm types.
 */
class LLVMTypeInfo : public TypeExtension {
 public:
  static const uint TYPE_EXTENSION_ID = 1;

  virtual ~LLVMTypeInfo();

  /**
   * Return the llvm::Type object corresponding to the parent FN::Type. Note, that llvm::Type
   * objects belong to some LLVMContext and therefore cannot be stored globally. Different
   * LLVMContexts exist when llvm is used from multiple threads at the same time.
   */
  virtual llvm::Type *get_type(llvm::LLVMContext &context) const = 0;

  /**
   * Build the code to create a copy of the given value. Since values are immutable in llvm, this
   * function can just return the original value. Only when it is e.g. a pointer to some outside
   * object, that has to be copied, a non trivial implementation has to be provided.
   */
  virtual llvm::Value *build_copy_ir(CodeBuilder &builder, llvm::Value *value) const = 0;

  /**
   * Build code to free the given value.
   */
  virtual void build_free_ir(CodeBuilder &builder, llvm::Value *value) const = 0;

  /**
   * Build code to relocate the value to a specific memory address. The original value in the
   * virtual register should be freed.
   *
   * Usually, it should be possible to interpret the stored bytes as C++ object.
   */
  virtual void build_store_ir__relocate(CodeBuilder &builder,
                                        llvm::Value *value,
                                        llvm::Value *address) const = 0;

  /**
   * Build code to copy the value to a specific memory address. The original value should stay the
   * same.
   *
   * Usually, it should be possible to interpret the stored bytes as C++ object.
   */
  virtual void build_store_ir__copy(CodeBuilder &builder,
                                    llvm::Value *value,
                                    llvm::Value *address) const = 0;

  /**
   * Build code to copy the value from a specific memory address into a llvm::Value. The stored
   * value should not be changed.
   */
  virtual llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const = 0;

  /**
   * Build code to relocate the value froma specific memory address into a llvm::Value. The stored
   * value should be freed in the process.
   */
  virtual llvm::Value *build_load_ir__relocate(CodeBuilder &builder,
                                               llvm::Value *address) const = 0;
};

/* Trivial: The type could be copied with memcpy
 *   and freeing the type does nothing.
 *   Subclasses still have to implement functions to
 *   store and load this type from memory. */
class TrivialLLVMTypeInfo : public LLVMTypeInfo {
 public:
  llvm::Value *build_copy_ir(CodeBuilder &builder, llvm::Value *value) const override;
  void build_free_ir(CodeBuilder &builder, llvm::Value *value) const override;
  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *address) const override;
  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *address) const override;
};

/* Packed: The memory layout in llvm matches
 *   the layout used in the rest of the C/C++ code.
 *   That means, that no special load/store functions
 *   have to be written. */
class PackedLLVMTypeInfo : public TrivialLLVMTypeInfo {
 private:
  typedef std::function<llvm::Type *(llvm::LLVMContext &context)> CreateFunc;
  CreateFunc m_create_func;

 public:
  PackedLLVMTypeInfo(CreateFunc create_func) : m_create_func(create_func)
  {
  }

  llvm::Type *get_type(llvm::LLVMContext &context) const override;
  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override;
  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *address) const override;
};

class PointerLLVMTypeInfo : public LLVMTypeInfo {
 private:
  typedef std::function<void *(void *)> CopyFunc;
  typedef std::function<void(void *)> FreeFunc;

  CopyFunc m_copy_func;
  FreeFunc m_free_func;

  static void *copy_value(PointerLLVMTypeInfo *info, void *value);
  static void free_value(PointerLLVMTypeInfo *info, void *value);

 public:
  PointerLLVMTypeInfo(CopyFunc copy_func, FreeFunc free_func)
      : m_copy_func(copy_func), m_free_func(free_func)
  {
  }

  llvm::Type *get_type(llvm::LLVMContext &context) const override;
  llvm::Value *build_copy_ir(CodeBuilder &builder, llvm::Value *value) const override;
  void build_free_ir(CodeBuilder &builder, llvm::Value *value) const override;
  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *address) const override;
  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *address) const override;
  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override;
  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *address) const override;
};

/**
 * Use this when the pointer is just referenced is owned by someone else.
 */
class PointerRefLLVMTypeInfo : public LLVMTypeInfo {
  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::Type::getInt8PtrTy(context);
  }

  llvm::Value *build_copy_ir(CodeBuilder &UNUSED(builder), llvm::Value *value) const override
  {
    return value;
  }

  void build_free_ir(CodeBuilder &UNUSED(builder), llvm::Value *UNUSED(value)) const override
  {
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    builder.CreateStore(value, addr);
  }

  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *address) const override
  {
    this->build_store_ir__copy(builder, value, address);
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    return builder.CreateLoad(addr);
  }

  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *address) const override
  {
    return this->build_load_ir__copy(builder, address);
  }
};

/**
 * Use this when the type is reference counted. Furthermore, the type has to be immutable when
 * object is owned by more than one.
 */
template<typename T> class SharedImmutablePointerLLVMTypeInfo : public LLVMTypeInfo {
 private:
  static T *copy_by_incrementing_refcount(T *value)
  {
    if (value == nullptr) {
      return nullptr;
    }
    else {
      value->incref();
      return value;
    }
  }

  static void free_by_decrementing_refcount(T *value)
  {
    if (value != nullptr) {
      value->decref();
    }
  }

 public:
  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::Type::getInt8PtrTy(context);
  }

  llvm::Value *build_copy_ir(CodeBuilder &builder, llvm::Value *value) const override
  {
    return builder.CreateCallPointer((void *)copy_by_incrementing_refcount,
                                     {value},
                                     builder.getAnyPtrTy(),
                                     "copy by incrementing refcount");
  }

  void build_free_ir(CodeBuilder &builder, llvm::Value *value) const override
  {
    builder.CreateCallPointer((void *)free_by_decrementing_refcount,
                              {value},
                              builder.getVoidTy(),
                              "free by decrementing refcount");
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *address) const override
  {
    auto copied_value = this->build_copy_ir(builder, value);
    this->build_store_ir__relocate(builder, copied_value, address);
  }

  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    builder.CreateStore(value, addr);
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    auto *value = builder.CreateLoad(addr);
    auto *copied_value = this->build_copy_ir(builder, value);
    return copied_value;
  }

  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    auto *value = builder.CreateLoad(addr);
    auto *nullptr_value = builder.getAnyPtr((void *)nullptr);
    builder.CreateStore(nullptr_value, addr);
    return value;
  }
};

/**
 * The type has to implement a clone() method.
 */
template<typename T> class OwningPointerLLVMTypeInfo : public LLVMTypeInfo {
 private:
  static T *copy_value(const T *value)
  {
    if (value == nullptr) {
      return nullptr;
    }
    else {
      return value->clone();
    }
  }

  static void free_value(T *value)
  {
    if (value != nullptr) {
      delete value;
    }
  }

 public:
  llvm::Type *get_type(llvm::LLVMContext &context) const override
  {
    return llvm::Type::getInt8PtrTy(context);
  }

  llvm::Value *build_copy_ir(CodeBuilder &builder, llvm::Value *value) const override
  {
    return builder.CreateCallPointer(
        (void *)copy_value, {value}, builder.getAnyPtrTy(), "copy pointer");
  }

  void build_free_ir(CodeBuilder &builder, llvm::Value *value) const override
  {
    builder.CreateCallPointer((void *)free_value, {value}, builder.getVoidTy(), "free pointer");
  }

  void build_store_ir__copy(CodeBuilder &builder,
                            llvm::Value *value,
                            llvm::Value *address) const override
  {
    auto *copied_value = this->build_copy_ir(builder, value);
    this->build_store_ir__relocate(builder, copied_value, address);
  }

  void build_store_ir__relocate(CodeBuilder &builder,
                                llvm::Value *value,
                                llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    builder.CreateStore(value, addr);
  }

  llvm::Value *build_load_ir__copy(CodeBuilder &builder, llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    auto *value = builder.CreateLoad(addr);
    auto *copied_value = this->build_copy_ir(builder, value);
    return copied_value;
  }

  llvm::Value *build_load_ir__relocate(CodeBuilder &builder, llvm::Value *address) const override
  {
    auto *addr = builder.CastToAnyPtrPtr(address);
    auto *value = builder.CreateLoad(addr);
    auto *nullptr_value = builder.getAnyPtr((void *)nullptr);
    builder.CreateStore(nullptr_value, addr);
    return value;
  }
};

inline llvm::Type *get_llvm_type(Type *type, llvm::LLVMContext &context)
{
  return type->extension<LLVMTypeInfo>().get_type(context);
}

Vector<llvm::Type *> types_of_type_infos(const Vector<LLVMTypeInfo *> &type_infos,
                                         llvm::LLVMContext &context);

} /* namespace FN */
