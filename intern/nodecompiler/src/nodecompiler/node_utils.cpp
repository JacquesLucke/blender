#include "core.hpp"

namespace LLVMNodeCompiler {

void ExecuteFunctionNode::set_execute_function(void *function, bool use_this)
{
	this->execute_function = function;
	this->use_this = use_this;
}

void ExecuteFunctionNode::buildIR(
	llvm::IRBuilder<> &builder,
	std::vector<llvm::Value *> &inputs,
	std::vector<llvm::Value *> &r_outputs) const
{
	assert(this->execute_function);

	llvm::LLVMContext &context = builder.getContext();

	std::vector<llvm::Type *> arg_types;
	std::vector<llvm::Value *> arguments;
	if (this->use_this) {
		arguments.push_back(voidPtrToIR(builder, this));
		arg_types.push_back(getVoidPtrTy(builder));
	}

	arguments.insert(arguments.end(), inputs.begin(), inputs.end());
	for (auto socket : this->inputs()) {
		arg_types.push_back(socket.type->getLLVMType(context));
	}

	std::vector<llvm::Value *> output_pointers;
	for (auto socket : this->outputs()) {
		llvm::Type *type = socket.type->getLLVMType(context);
		arg_types.push_back(type->getPointerTo());
		llvm::Value *alloced_ptr = builder.CreateAlloca(type);
		output_pointers.push_back(alloced_ptr);
		arguments.push_back(alloced_ptr);
	}

	llvm::FunctionType *ftype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(context), arg_types, false);
	callPointer(builder, this->execute_function, ftype, arguments);

	for (auto output_pointer : output_pointers) {
		llvm::Value *result = builder.CreateLoad(output_pointer);
		r_outputs.push_back(result);
	}
}

} /* namespace LLVMNodeCompiler */