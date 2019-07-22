#include "builder.hpp"
#include "BLI_string.h"
#include "llvm/IR/TypeBuilder.h"
#include "FN_tuple_call.hpp"

namespace FN {

Vector<llvm::Type *> CodeBuilder::types_of_values(ArrayRef<llvm::Value *> values)
{
  Vector<llvm::Type *> types;
  for (llvm::Value *value : values) {
    types.append(value->getType());
  }
  return types;
}

static llvm::Function *create_wrapper_function(llvm::Module *module,
                                               llvm::FunctionType *ftype,
                                               void *func_ptr,
                                               const char *name)
{
  llvm::Function *function = llvm::Function::Create(
      ftype, llvm::GlobalValue::LinkageTypes::InternalLinkage, name, module);

  llvm::BasicBlock *bb = llvm::BasicBlock::Create(module->getContext(), "entry", function);
  llvm::IRBuilder<> builder(bb);

  Vector<llvm::Value *> args;
  for (auto &arg : function->args()) {
    args.append(&arg);
  }

  llvm::Value *address_int = builder.getInt64((size_t)func_ptr);
  llvm::Value *address = builder.CreateIntToPtr(address_int, ftype->getPointerTo());
  llvm::Value *result = builder.CreateCall(address, to_llvm_array_ref(args));

  if (ftype->getReturnType() == builder.getVoidTy()) {
    builder.CreateRetVoid();
  }
  else {
    builder.CreateRet(result);
  }
  return function;
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            llvm::FunctionType *ftype,
                                            ArrayRef<llvm::Value *> args,
                                            const char *function_name)
{
  BLI_assert(!ftype->isVarArg());
  char name[64];
  BLI_snprintf(name, sizeof(name), "%s (%p)", function_name, func_ptr);

  llvm::Module *module = this->getModule();
  llvm::Function *wrapper_function = module->getFunction(name);
  if (wrapper_function == nullptr) {
    wrapper_function = create_wrapper_function(module, ftype, func_ptr, name);
  }

  return m_builder.CreateCall(wrapper_function, to_llvm_array_ref(args));
}

llvm::Value *CodeBuilder::CreateCallPointer(void *func_ptr,
                                            ArrayRef<llvm::Value *> args,
                                            llvm::Type *return_type,
                                            const char *function_name)
{
  Vector<llvm::Type *> arg_types = this->types_of_values(args);
  llvm::FunctionType *ftype = llvm::FunctionType::get(
      return_type, to_llvm_array_ref(arg_types), false);
  return this->CreateCallPointer(func_ptr, ftype, args, function_name);
}

static void assert_impl(bool value, const char *message)
{
  if (!value) {
    std::cout << "Assert Message: " << message << std::endl;
    BLI_assert(false);
  }
}

void CodeBuilder::CreateAssert(llvm::Value *condition, const char *message)
{
  llvm::Value *condition_as_byte = this->CreateCastIntTo8(condition, false);
  llvm::Value *message_ptr = this->getInt8Ptr(message);
  this->CreateCallPointer(
      (void *)assert_impl, {condition_as_byte, message_ptr}, this->getVoidTy(), "Assert");
}

void CodeBuilder::CreateAssertFalse(const char *message)
{
  llvm::Value *condition = this->getInt1(false);
  this->CreateAssert(condition, message);
}

/* Printing
 **********************************/

void CodeBuilder::CreatePrintf(const char *format, ArrayRef<llvm::Value *> values)
{
  llvm::FunctionType *printf_ftype = llvm::TypeBuilder<int(char *, ...), false>::get(
      this->getContext());

  llvm::Function *printf_func = llvm::cast<llvm::Function>(
      this->getModule()->getOrInsertFunction("printf", printf_ftype));
  printf_func->addParamAttr(0, llvm::Attribute::NoAlias);

  Vector<llvm::Value *> args;
  args.append(this->getInt8Ptr(format));
  for (llvm::Value *arg : values) {
    llvm::Value *passed_arg = arg;
    if (arg->getType()->isFloatTy()) {
      passed_arg = this->CastFloatToDouble(arg);
    }
    args.append(passed_arg);
  }
  m_builder.CreateCall(printf_func, to_llvm_array_ref(args));
}

static void print_stacktrace(ExecutionContext *context)
{
  context->stack().print_traceback();
}

void CodeBuilder::CreatePrintfWithStacktrace(llvm::Value *context_ptr,
                                             const char *format,
                                             ArrayRef<llvm::Value *> values)
{
  this->CreateCallPointer(
      (void *)print_stacktrace, {context_ptr}, this->getVoidTy(), "Print Stacktrace");
  this->CreatePrintf("-> ");
  this->CreatePrintf(format, values);
  this->CreatePrintf("\n");
}

/* For Loop
 ******************************************/

IRConstruct_ForLoop CodeBuilder::CreateForLoop(StringRef name)
{
  auto entry_block = this->NewBlockInFunction(name + " Entry");
  auto condition_block = this->NewBlockInFunction(name + " Condition");
  auto body_block = this->NewBlockInFunction(name + " Body");

  CodeBuilder entry_builder(entry_block);
  CodeBuilder condition_builder(condition_block);
  CodeBuilder body_builder(body_block);

  this->CreateBr(entry_block);

  return IRConstruct_ForLoop(entry_builder, condition_builder, body_builder);
}

void IRConstruct_ForLoop::finalize(CodeBuilder &after_builder, llvm::Value *condition)
{
  m_entry.CreateBr(m_condition_entry);
  m_body.CreateBr(m_condition_entry);

  auto after_block = m_entry.NewBlockInFunction("After Loop");
  m_condition.CreateCondBr(condition, m_body_entry, after_block);
  after_builder.SetInsertPoint(after_block);
}

/* Iterations Loop
 **************************************/

IRConstruct_IterationsLoop CodeBuilder::CreateNIterationsLoop(llvm::Value *iterations,
                                                              StringRef name)
{
  BLI_assert(iterations->getType()->isIntegerTy());

  IRConstruct_ForLoop loop = this->CreateForLoop(name);
  CodeBuilder &condition_builder = loop.condition_builder();

  llvm::PHINode *current_iteration = condition_builder.CreatePhi(iterations->getType(), 2);

  return IRConstruct_IterationsLoop(loop, iterations, current_iteration);
}

void IRConstruct_IterationsLoop::finalize(CodeBuilder &after_builder)
{
  CodeBuilder &entry_builder = m_loop.entry_builder();
  CodeBuilder &condition_builder = m_loop.condition_builder();
  CodeBuilder &body_builder = m_loop.body_builder();

  llvm::Value *next_iteration = body_builder.CreateIAdd(m_current_iteration, 1);
  m_current_iteration->addIncoming(entry_builder.getInt32(0), entry_builder.GetInsertBlock());
  m_current_iteration->addIncoming(next_iteration, body_builder.GetInsertBlock());
  llvm::Value *condition = condition_builder.CreateICmpULT(m_current_iteration, m_iterations);
  m_loop.finalize(after_builder, condition);
}

/* If Then Else
 ************************************/

IRConstruct_IfThenElse CodeBuilder::CreateIfThenElse(llvm::Value *condition, StringRef name)
{
  auto then_block = this->NewBlockInFunction(name + " Then");
  auto else_block = this->NewBlockInFunction(name + " Else");
  this->CreateCondBr(condition, then_block, else_block);
  return IRConstruct_IfThenElse(CodeBuilder(then_block), CodeBuilder(else_block));
}

void IRConstruct_IfThenElse::finalize(CodeBuilder &after_builder)
{
  auto after_block = m_then_builder.NewBlockInFunction("After If");
  m_then_builder.CreateBr(after_block);
  m_else_builder.CreateBr(after_block);
  after_builder.SetInsertPoint(after_block);
}

} /* namespace FN */
