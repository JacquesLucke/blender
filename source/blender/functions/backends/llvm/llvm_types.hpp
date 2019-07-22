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
  BLI_COMPOSITION_DECLARATION(LLVMTypeInfo);

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
  typedef std::function<void *()> DefaultFunc;

  CopyFunc m_copy_func;
  FreeFunc m_free_func;
  DefaultFunc m_default_func;

  static void *copy_value(PointerLLVMTypeInfo *info, void *value);
  static void free_value(PointerLLVMTypeInfo *info, void *value);
  static void *default_value(PointerLLVMTypeInfo *info);

 public:
  PointerLLVMTypeInfo(CopyFunc copy_func, FreeFunc free_func, DefaultFunc default_func)
      : m_copy_func(copy_func), m_free_func(free_func), m_default_func(default_func)
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

inline LLVMTypeInfo *get_type_info(const SharedType &type)
{
  auto ext = type->extension<LLVMTypeInfo>();
  BLI_assert(ext);
  return ext;
}

inline llvm::Type *get_llvm_type(SharedType &type, llvm::LLVMContext &context)
{
  return get_type_info(type)->get_type(context);
}

LLVMTypes types_of_type_infos(const Vector<LLVMTypeInfo *> &type_infos,
                              llvm::LLVMContext &context);

} /* namespace FN */
