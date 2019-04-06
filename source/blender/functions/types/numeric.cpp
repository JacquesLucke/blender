#include "numeric.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

namespace FN { namespace Types {

	LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_float)
	{
		SharedType type = SharedType::New("Float");
		type->extend(new CPPTypeInfoForType<float>());
		type->extend(new SimpleLLVMTypeInfo([](llvm::LLVMContext &context) {
			return llvm::Type::getFloatTy(context);
		}));
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_int32)
	{
		SharedType type = SharedType::New("Int32");
		type->extend(new CPPTypeInfoForType<int32_t>());
		type->extend(new SimpleLLVMTypeInfo([](llvm::LLVMContext &context) {
			return llvm::Type::getIntNTy(context, 32);
		}));
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, GET_TYPE_fvec3)
	{
		SharedType type = SharedType::New("FVec3");
		type->extend(new CPPTypeInfoForType<Vector>());
		type->extend(new SimpleLLVMTypeInfo([](llvm::LLVMContext &context) {
			llvm::Type *base = llvm::Type::getFloatTy(context);
			return llvm::StructType::get(context, {base, base, base}, true);
		}));
		return type;
	}

} } /* namespace FN::Types */