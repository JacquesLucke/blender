/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <iostream>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>

#include "FN_llvm.hh"

#include "BLI_vector.hh"

namespace blender::fn {

void playground()
{
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("My Module", context);

  llvm::Type *ret_type = llvm::Type::getInt32Ty(context);
  llvm::FunctionType *function_type = llvm::FunctionType::get(ret_type, false);
  llvm::Function *function = llvm::Function::Create(
      function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage, "My Func", *module);
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
  llvm::IRBuilder<> builder{bb};
  llvm::Value *value = builder.getInt32(42);
  builder.CreateRet(value);

  BLI_assert(llvm::verifyModule(*module, &llvm::outs()));

  llvm::ExecutionEngine *ee = llvm::EngineBuilder(std::move(module)).create();
  ee->finalizeObject();

  const uint64_t function_ptr = ee->getFunctionAddress(function->getName().str());
  using FuncType = int (*)();
  const FuncType generated_function = (FuncType)function_ptr;
  const int result = generated_function();
  std::cout << result << "\n";

  // function->dump();
}

}  // namespace blender::fn
