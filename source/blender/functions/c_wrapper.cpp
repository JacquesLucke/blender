#include "FN_functions.h"
#include "FN_functions.hpp"

#include "./types/types.hpp"

inline FN::CPUFunction *unwrap(FnCPUFunction fn)
{
	return (FN::CPUFunction *)fn;
}

inline FN::Tuple *unwrap(FnTuple tuple)
{
	return (FN::Tuple *)tuple;
}

inline FnTuple wrap(FN::Tuple *tuple)
{
	return (FnTuple)tuple;
}

void FN_function_call(FnCPUFunction fn, FnTuple fn_in, FnTuple fn_out)
{
	unwrap(fn)->call(*unwrap(fn_in), *unwrap(fn_out));
}

FnTuple FN_tuple_for_input(FnCPUFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->signature().inputs());
	return wrap(tuple);
}

FnTuple FN_tuple_for_output(FnCPUFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->signature().outputs());
	return wrap(tuple);
}

void FN_tuple_free(FnTuple tuple)
{
	delete unwrap(tuple);
}

void FN_tuple_set_float(FnTuple tuple, uint index, float value)
{
	unwrap(tuple)->set<float>(index, value);
}

struct Vector {
	float x, y, z;
};

void FN_tuple_set_float_vector_3(FnTuple tuple, uint index, float value[3])
{
	unwrap(tuple)->set<Vector>(index, *(Vector *)value);
}

void FN_tuple_get_float_vector_3(FnTuple tuple, uint index, float dst[3])
{
	*(Vector *)dst = unwrap(tuple)->get<Vector>(index);
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
#include <algorithm>

class DeformFunction : public FN::CPUFunction {
private:
	DeformFunction(FN::Signature sig)
		: CPUFunction(sig) {}

public:
	static DeformFunction *Create()
	{
		FN::Signature sig({FN::Types::floatvec3d_ty, FN::Types::float_ty}, {FN::Types::floatvec3d_ty});
		return new DeformFunction(sig);
	}

	void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) override
	{
		Vector vec = fn_in.get<Vector>(0);
		float control = fn_in.get<float>(1);

		Vector result;

		result.x = vec.x * control;
		result.y = vec.y / std::max(control, 0.1f);
		result.z = vec.z;

		fn_out.set<Vector>(0, result);
	}
};

FnCPUFunction FN_get_deform_function()
{
	return (FnCPUFunction)DeformFunction::Create();
}