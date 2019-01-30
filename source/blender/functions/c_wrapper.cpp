#include "FN_functions.h"
#include "FN_functions.hpp"

#include "./types/types.hpp"

#define WRAPPERS(T1, T2) \
	inline T1 unwrap(T2 value) { return (T1)value; } \
	inline T2 wrap(T1 value) { return (T2)value; }

WRAPPERS(const FN::Function *, FnFunction);
WRAPPERS(FN::Tuple *, FnTuple);
WRAPPERS(const FN::TupleCallBody *, FnCallTuple);

void FN_function_call_tuple(FnCallTuple fn_call, FnTuple fn_in, FnTuple fn_out)
{
	unwrap(fn_call)->call(*unwrap(fn_in), *unwrap(fn_out));
}

FnCallTuple FN_function_get_tuple_call(FnFunction fn)
{
	return wrap(unwrap(fn)->body<FN::TupleCallBody>());
}

FnTuple FN_tuple_for_input(FnFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->signature().inputs());
	return wrap(tuple);
}

FnTuple FN_tuple_for_output(FnFunction fn)
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



class Deform1 : public FN::TupleCallBody {
public:
	virtual void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
	{
		Vector vec = fn_in.get<Vector>(0);
		float control = fn_in.get<float>(1);

		Vector result;

		result.x = vec.x * control;
		result.y = vec.y;// / std::max(control, 0.1f);
		result.z = vec.z;

		fn_out.set<Vector>(0, result);
	}
};

class Deform2 : public FN::TupleCallBody {
public:
	virtual void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
	{
		Vector vec = fn_in.get<Vector>(0);
		float control = fn_in.get<float>(1);

		Vector result;

		result.x = vec.x;
		result.y = vec.y * control;// / std::max(control, 0.1f);
		result.z = vec.z;

		fn_out.set<Vector>(0, result);
	}
};

FnFunction FN_get_deform_function(int type)
{
	FN::Signature signature({FN::Types::floatvec3d_ty, FN::Types::float_ty}, {FN::Types::floatvec3d_ty});
	FN::FunctionBodies bodies;
	if (type == 0) bodies.add(new Deform1());
	else bodies.add(new Deform2());

	return wrap(new FN::Function(signature, bodies));
}