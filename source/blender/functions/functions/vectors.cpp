#include "vectors.hpp"
#include "FN_types.hpp"

#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "BLI_lazy_init.hpp"
#include "BLI_math.h"

namespace FN { namespace Functions {

	using namespace Types;

	class CombineVector : public LLVMBuildIRBody {
		void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &outputs) const override
		{
			llvm::Type *vector_ty = get_llvm_type(
				get_fvec3_type(), builder.getContext());

			llvm::Value *vector = llvm::UndefValue::get(vector_ty);
			vector = builder.CreateInsertValue(vector, inputs[0], 0);
			vector = builder.CreateInsertValue(vector, inputs[1], 1);
			vector = builder.CreateInsertValue(vector, inputs[2], 2);
			outputs.append(vector);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, combine_vector)
	{
		auto fn = SharedFunction::New("Combine Vector", Signature({
			InputParameter("X", get_float_type()),
			InputParameter("Y", get_float_type()),
			InputParameter("Z", get_float_type()),
		}, {
			OutputParameter("Vector", get_fvec3_type()),
		}));
		fn->add_body(new CombineVector());
		return fn;
	}

	class SeparateVector : public LLVMBuildIRBody {
		void build_ir(
			llvm::IRBuilder<> &builder,
			const LLVMValues &inputs,
			LLVMValues &r_outputs) const override
		{
			r_outputs.append(builder.CreateExtractValue(inputs[0], 0));
			r_outputs.append(builder.CreateExtractValue(inputs[0], 1));
			r_outputs.append(builder.CreateExtractValue(inputs[0], 2));
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, separate_vector)
	{
		auto fn = SharedFunction::New("Separate Vector", Signature({
			InputParameter("Vector", get_fvec3_type()),
		}, {
			OutputParameter("X", get_float_type()),
			OutputParameter("Y", get_float_type()),
			OutputParameter("Z", get_float_type()),
		}));
		fn->add_body(new SeparateVector());
		return fn;
	}


	class VectorDistance : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector a = fn_in.get<Vector>(0);
			Vector b = fn_in.get<Vector>(1);
			float distance = len_v3v3((float *)&a, (float *)&b);
			fn_out.set<float>(0, distance);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, vector_distance)
	{
		auto fn = SharedFunction::New("Vector Distance", Signature({
			InputParameter("A", get_fvec3_type()),
			InputParameter("B", get_fvec3_type()),
		}, {
			OutputParameter("Distance", get_float_type()),
		}));
		fn->add_body(new VectorDistance());
		return fn;
	}

} } /* namespace FN::Functions */