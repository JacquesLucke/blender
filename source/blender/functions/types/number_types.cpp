#include "nodecompiler/core.hpp"

namespace NC = LLVMNodeCompiler;

class IntegerType : public NC::Type {
public:
	IntegerType(uint bits)
		: bits(bits) {}

	llvm::Type *createLLVMType(llvm::LLVMContext &context)
	{
		return llvm::Type::getIntNTy(context, this->bits);
	}

private:
	uint bits;
};

class FloatType : public NC::Type {
public:
	FloatType() {}

	llvm::Type *createLLVMType(llvm::LLVMContext &context)
	{
		return llvm::Type::getFloatTy(context);
	}
};

auto type_int32 = new IntegerType(32);
auto type_float = new FloatType();
