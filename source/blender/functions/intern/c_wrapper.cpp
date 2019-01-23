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

void FN_inputs_set_index(FnInputsRef fn_in, uint index, void *src)
{
	((FN::Inputs *)fn_in)->set(index, src);
}

void FN_outputs_get_index(FnOutputsRef fn_out, uint index, void *dst)
{
	((FN::Outputs *)fn_out)->get(index, dst);
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


class AddConstFunction : public FN::Function {
private:
	AddConstFunction(FN::Signature sig, int value)
		: Function(sig), value(value) {}

public:
	static AddConstFunction *Create(int value)
	{
		FN::Signature sig({FN::Types::int32_ty}, {FN::Types::int32_ty});
		return new AddConstFunction(sig, value);
	}

	bool call(const FN::Inputs &fn_in, FN::Outputs &fn_out) override
	{
		int a;
		fn_in.get(0, &a);
		int result = a + this->value;
		fn_out.set(0, &result);
		return true;
	}

private:
	int value;
};

FunctionRef FN_get_add_const_function(int value)
{
	return (FunctionRef)AddConstFunction::Create(value);
}

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
		float vec[3];
		float control;
		fn_in.get(0, vec);
		fn_in.get(1, &control);

		float result[3];

		result[0] = vec[0] * control;
		result[1] = vec[1] + std::sin(control);
		result[2] = vec[2];

		fn_out.set(0, result);

		return true;
	}
};

FunctionRef FN_get_deform_function()
{
	return (FunctionRef)DeformFunction::Create();
}