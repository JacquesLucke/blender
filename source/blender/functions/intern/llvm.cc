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

#include <fstream>
#include <iostream>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

#include "FN_llvm.hh"

#include "BLI_vector.hh"

namespace blender::fn {

  const std::string object_file_path = "C:\\Users\\jacques\\Documents\\my_object.o";

class MyObjectCache : public llvm::ObjectCache {
  void notifyObjectCompiled(const llvm::Module *module, llvm::MemoryBufferRef obj) override
  {
    std::cout << "Compiled Module: " << module->getName().str() << "   -    "
              << module->getModuleIdentifier() << "\n";
    std::cout << "Size: " << obj.getBufferSize() << "\n";

    std::ofstream f;    
    f.open(object_file_path, std::ofstream::out | std::ofstream::binary);
    f.write(obj.getBufferStart(), obj.getBufferSize());
    f.close();
  }

  std::unique_ptr<llvm::MemoryBuffer> getObject(const llvm::Module *module) override
  {
    //return {};
    std::cout << "Request Cache: " << module->getName().str() << "   -    "
              << module->getModuleIdentifier() << "\n";
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer_err = llvm::MemoryBuffer::getFile(object_file_path);
    std::unique_ptr<llvm::MemoryBuffer> buffer = std::move(*buffer_err);
    return buffer;
  }
};

void playground()
{
  static bool initialized = []() {
    /* Set assembly syntax flavour. */
    char const *args[] = {"some-random-name-for-the-parser", "--x86-asm-syntax=intel"};
    llvm::cl::ParseCommandLineOptions(std::size(args), args);

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    return true;
  }();

  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("My Module", context);

  llvm::Type *int32_type = llvm::Type::getInt32Ty(context);
  llvm::FunctionType *function_type = llvm::FunctionType::get(
      int32_type, {int32_type, int32_type}, false);
  llvm::Function *function = llvm::Function::Create(
      function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage, "My Func", *module);
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "entry", function);
  llvm::IRBuilder<> builder{bb};
  llvm::Value *sum_value = builder.CreateAdd(function->getArg(0), function->getArg(1));
  llvm::Value *product_value = builder.CreateMul(function->getArg(0), sum_value);
  builder.CreateRet(product_value);

  BLI_assert(!llvm::verifyModule(*module, &llvm::outs()));

  MyObjectCache object_cache;

  llvm::Module *module_ptr = &*module;
  std::unique_ptr<llvm::ExecutionEngine> ee{llvm::EngineBuilder(std::move(module)).create()};
  ee->setObjectCache(&object_cache);
  ee->finalizeObject();

  const uint64_t function_ptr = ee->getFunctionAddress(function->getName().str());
  using FuncType = int (*)(int, int);
  const FuncType generated_function = (FuncType)function_ptr;
  const int result = generated_function(3, 5);
  std::cout << result << "\n";

  /*
  LLVMTargetMachineEmitToFile((LLVMTargetMachineRef)ee->getTargetMachine(),
                              llvm::wrap(module_ptr),
                              (char *)"C:\\Users\\jacques\\Documents\\machine_code.txt",
                              LLVMAssemblyFile,
                              nullptr);
  */
  // function->dump();
  function->print(llvm::outs());
}

}  // namespace blender::fn
