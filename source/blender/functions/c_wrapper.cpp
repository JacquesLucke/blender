#include "FN_functions.h"
#include "FN_functions.hpp"

#include "./types/types.hpp"

#include <iostream>

#define WRAPPERS(T1, T2) \
	inline T1 unwrap(T2 value) { return (T1)value; } \
	inline T2 wrap(T1 value) { return (T2)value; }


WRAPPERS(BLI::RefCounted<FN::Function> *, FnFunction);
WRAPPERS(BLI::RefCounted<const FN::Type> *, FnType);

WRAPPERS(FN::Tuple *, FnTuple);
WRAPPERS(const FN::TupleCallBody *, FnCallable);

void FN_function_call(FnCallable fn_call, FnTuple fn_in, FnTuple fn_out)
{
	unwrap(fn_call)->call(*unwrap(fn_in), *unwrap(fn_out));
}

FnCallable FN_function_get_callable(FnFunction fn)
{
	return wrap(unwrap(fn)->ptr()->body<FN::TupleCallBody>());
}

void FN_function_free(FnFunction fn)
{
	unwrap(fn)->decref();
}

FnTuple FN_tuple_for_input(FnFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->ptr()->signature().input_types());
	return wrap(tuple);
}

FnTuple FN_tuple_for_output(FnFunction fn)
{
	auto tuple = new FN::Tuple(unwrap(fn)->ptr()->signature().output_types());
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

const char *FN_type_name(FnType type)
{
	return ((FN::Type *)type)->name().c_str();
}

void FN_type_free(FnType type)
{
	unwrap(type)->decref();
}

static FnType get_type_with_increased_refcount(const FN::SharedType &type)
{
	BLI::RefCounted<const FN::Type> *typeref = type.refcounter();
	typeref->incref();
	return wrap(typeref);
}

FnType FN_type_get_float()
{
	return get_type_with_increased_refcount(FN::Types::float_ty);
}

FnType FN_type_get_int32()
{
	return get_type_with_increased_refcount(FN::Types::int32_ty);
}

FnType FN_type_get_float_vector_3d()
{
	return get_type_with_increased_refcount(FN::Types::floatvec3d_ty);
}


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

static FN::SharedFunction get_pass_through_float_function()
{
	FN::InputParameters inputs;
	inputs.append(FN::InputParameter("In", FN::Types::float_ty));

	FN::OutputParameters outputs;
	outputs.append(FN::OutputParameter("Out", FN::Types::float_ty));

	auto fn = FN::SharedFunction::New(FN::Signature(inputs, outputs), "Pass Through");
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

	auto fn = FN::SharedFunction::New(FN::Signature(inputs, outputs), "Deform");
	if (type == 0) {
		fn->add_body(new Deform1());
	}
	else {
		fn->add_body(new Deform2());
	}

	FN::DataFlowGraph graph;
	const FN::Node *n1 = graph.insert(fn);
	const FN::Node *n2 = graph.insert(fn);
	const FN::Node *n3 = graph.insert(fn);
	const FN::Node *n4 = graph.insert(fn);
	const FN::Node *p = graph.insert(get_pass_through_float_function());
	graph.link(n1->output(0), n2->input(0));
	graph.link(n2->output(0), n3->input(0));
	graph.link(n2->output(0), n4->input(0));
	graph.link(p->output(0), n1->input(1));
	graph.link(p->output(0), n2->input(1));
	graph.link(p->output(0), n3->input(1));
	graph.link(p->output(0), n4->input(1));

	std::string dot = graph.to_dot();
	//std::cout << dot << std::endl;

	BLI::RefCounted<FN::Function> *fn_ref = fn.refcounter();
	fn_ref->incref();
	return wrap(fn_ref);
}