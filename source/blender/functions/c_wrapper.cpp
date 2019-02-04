#include "FN_functions.h"
#include "FN_functions.hpp"

#include "./types/types.hpp"

#include <iostream>

#define WRAPPERS(T1, T2) \
	inline T1 unwrap(T2 value) { return (T1)value; } \
	inline T2 wrap(T1 value) { return (T2)value; }

WRAPPERS(const FN::Function *, FnFunction);
WRAPPERS(FN::Tuple *, FnTuple);
WRAPPERS(const FN::TupleCallBody *, FnCallable);

void FN_function_call(FnCallable fn_call, FnTuple fn_in, FnTuple fn_out)
{
	unwrap(fn_call)->call(*unwrap(fn_in), *unwrap(fn_out));
}

FnCallable FN_function_get_callable(FnFunction fn)
{
	return wrap(unwrap(fn)->body<FN::TupleCallBody>());
}

FnTuple FN_tuple_for_input(FnFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->signature().input_types());
	return wrap(tuple);
}

FnTuple FN_tuple_for_output(FnFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->signature().output_types());
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

template<typename T>
class PassThroughBody : public FN::TupleCallBody {
public:
	virtual void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
	{
		fn_out.set<T>(0, fn_in.get<T>(0));
	}
};

static FN::Function *get_pass_through_float_function()
{
	FN::InputParameters inputs;
	inputs.append(FN::InputParameter("In", FN::Types::float_ty));

	FN::OutputParameters outputs;
	outputs.append(FN::OutputParameter("Out", FN::Types::float_ty));

	auto fn = new FN::Function(FN::Signature(inputs, outputs), "Pass Through");
	fn->add_body(new PassThroughBody<float>());
	return fn;
}

FnFunction FN_get_deform_function(int type)
{
	FN::InputParameters inputs;
	inputs.append(FN::InputParameter("Position", FN::Types::floatvec3d_ty));
	inputs.append(FN::InputParameter("Control", FN::Types::float_ty));

	FN::OutputParameters outputs;
	outputs.append(FN::OutputParameter("Position", FN::Types::floatvec3d_ty));

	auto fn = new FN::Function(FN::Signature(inputs, outputs), "Deform");
	if (type == 0) {
		fn->add_body(new Deform1());
	}
	else {
		fn->add_body(new Deform2());
	}

	FN::DataFlowGraph graph;
	const FN::Node *n1 = graph.insert(*fn);
	const FN::Node *n2 = graph.insert(*fn);
	const FN::Node *n3 = graph.insert(*fn);
	const FN::Node *n4 = graph.insert(*fn);
	const FN::Node *p = graph.insert(*get_pass_through_float_function());
	graph.link(n1->output(0), n2->input(0));
	graph.link(n2->output(0), n3->input(0));
	graph.link(n2->output(0), n4->input(0));
	graph.link(p->output(0), n1->input(1));
	graph.link(p->output(0), n2->input(1));
	graph.link(p->output(0), n3->input(1));
	graph.link(p->output(0), n4->input(1));

	std::string dot = graph.to_dot();
	//std::cout << dot << std::endl;

	return wrap(fn);
}