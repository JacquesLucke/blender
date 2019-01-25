#include "FN_functions.h"
#include "FN_functions.hpp"

#include "./types/types.hpp"

bool FN_function_call(FunctionRef fn, FnInputsRef fn_in, FnOutputsRef fn_out)
{
	return ((FN::Function *)fn)->call(*(FN::Inputs *)fn_in, *(FN::Outputs *)fn_out);
}

FnInputsRef FN_inputs_new(FunctionRef fn)
{
	return (FnInputsRef)new FN::Inputs(*(FN::Function *)fn);
}

FnOutputsRef FN_outputs_new(FunctionRef fn)
{
	return (FnOutputsRef)new FN::Outputs(*(FN::Function *)fn);
}

void FN_inputs_free(FnInputsRef fn_in)
{
	delete (FN::Inputs *)fn_in;
}

void FN_outputs_free(FnOutputsRef fn_out)
{
	delete (FN::Outputs *)fn_out;
}

void FN_inputs_set_float(FnInputsRef fn_in, uint index, float value)
{
	((FN::Inputs *)fn_in)->set<float>(index, value);
}

struct Vector {
	float x, y, z;
};

void FN_inputs_set_float_vector_3(FnInputsRef fn_in, uint index, float value[3])
{
	((FN::Inputs *)fn_in)->set<Vector>(index, *(Vector *)value);
}

void FN_outputs_get_float_vector_3(FnOutputsRef fn_out, uint index, float dst[3])
{
	FN::Outputs *fn_out_ = (FN::Outputs *)fn_out;
	*(Vector *)dst = fn_out_->get<Vector>(index);
}

const char *FN_type_name(FnTypeRef type)
{
	return ((FN::Type *)type)->name().c_str();
}

FnTypeRef FN_type_get_float()
{ return (FnTypeRef)FN::Types::float_ty; }

FnTypeRef FN_type_get_int32()
{ return (FnTypeRef)FN::Types::int32_ty; }

FnTypeRef FN_type_get_float_vector_3d()
{ return (FnTypeRef)FN::Types::floatvec3d_ty; }


#include <cmath>

class DeformFunction : public FN::Function {
private:
	DeformFunction(FN::Signature sig)
		: Function(sig) {}

public:
	static DeformFunction *Create()
	{
		FN::Signature sig({FN::Types::floatvec3d_ty, FN::Types::float_ty}, {FN::Types::floatvec3d_ty});
		return new DeformFunction(sig);
	}

	bool call(const FN::Inputs &fn_in, FN::Outputs &fn_out) override
	{
		Vector vec = fn_in.get<Vector>(0);
		float control = fn_in.get<float>(1);

		Vector result;

		result.x = vec.x * control;
		result.y = vec.y;
		result.z = vec.z;

		fn_out.set<Vector>(0, result);

		return true;
	}
};

FunctionRef FN_get_deform_function()
{
	return (FunctionRef)DeformFunction::Create();
}