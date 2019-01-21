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

void FN_inputs_set_index(FnInputsRef fn_in, uint index, void *value)
{
	((FN::Inputs *)fn_in)->set(index, value);
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
public:
	AddConstFunction(int value)
		: value(value)
	{
		this->m_signature = FN::Signature({FN::Types::int32_ty}, {FN::Types::int32_ty});
	}

	bool call(FN::Inputs &UNUSED(fn_in), FN::Outputs &UNUSED(fn_out))
	{
		return false;
	}

private:
	int value;
};

FunctionRef FN_get_add_const_function(int value)
{
	return (FunctionRef)new AddConstFunction(value);
}