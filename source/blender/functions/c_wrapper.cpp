#include "FN_functions.h"
#include "FN_functions.hpp"

#include "./types/types.hpp"

#include <iostream>

#define WRAPPERS(T1, T2) \
	inline T1 unwrap(T2 value) { return (T1)value; } \
	inline T2 wrap(T1 value) { return (T2)value; }


WRAPPERS(BLI::RefCounted<FN::Function> *, FnFunction);
WRAPPERS(BLI::RefCounted<FN::Type> *, FnType);

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
	return unwrap(type)->ptr()->name().c_str();
}

void FN_type_free(FnType type)
{
	unwrap(type)->decref();
}

static FnType get_type_with_increased_refcount(const FN::SharedType &type)
{
	BLI::RefCounted<FN::Type> *typeref = type.refcounter();
	typeref->incref();
	return wrap(typeref);
}

#define SIMPLE_TYPE_GETTER(name) \
	FnType FN_type_get_##name() \
	{ return get_type_with_increased_refcount(FN::Types::get_##name##_type()); }

SIMPLE_TYPE_GETTER(float);
SIMPLE_TYPE_GETTER(int32);
SIMPLE_TYPE_GETTER(fvec3);

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
		result.y = vec.y * control;
		result.z = vec.z;

		fn_out.set<Vector>(0, result);
	}
};

class PassThroughFloat : public FN::TupleCallBody {
public:
	virtual void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
	{
		fn_out.set<float>(0, fn_in.get<float>(0));
	}
};

static FN::SharedFunction get_deform_function(int type)
{
	FN::InputParameters inputs;
	inputs.append(FN::InputParameter("Position", FN::Types::get_fvec3_type()));
	inputs.append(FN::InputParameter("Control", FN::Types::get_float_type()));

	FN::OutputParameters outputs;
	outputs.append(FN::OutputParameter("Position", FN::Types::get_fvec3_type()));

	auto fn = FN::SharedFunction::New(FN::Signature(inputs, outputs), "Deform");
	if (type == 0) {
		fn->add_body(new Deform1());
	}
	else {
		fn->add_body(new Deform2());
	}
	return fn;
}

static FN::SharedFunction get_pass_through_float_function()
{
	FN::InputParameters inputs = {FN::InputParameter("In", FN::Types::get_float_type())};
	FN::OutputParameters outputs = {FN::OutputParameter("Out", FN::Types::get_float_type())};
	auto fn = FN::SharedFunction::New(FN::Signature(inputs, outputs), "Pass Through");
	fn->add_body(new PassThroughFloat());
	return fn;
}

FnFunction FN_get_deform_function(int type)
{
	auto fn = get_deform_function(type);
	BLI::RefCounted<FN::Function> *fn_ref = fn.refcounter();
	fn_ref->incref();
	return wrap(fn_ref);
}

FnFunction FN_get_generated_function()
{
	FN::SharedDataFlowGraph graph = FN::SharedDataFlowGraph::New();

	FN::SharedFunction f1 = get_deform_function(0);
	FN::SharedFunction f2 = get_deform_function(1);
	FN::SharedFunction pass = get_pass_through_float_function();

	auto n1 = graph->insert(f1);
	auto n2 = graph->insert(f2);
	auto npass = graph->insert(pass);

	graph->link(n1->output(0), n2->input(0));
	graph->link(npass->output(0), n1->input(1));
	graph->link(npass->output(0), n2->input(1));

	auto fn = FN::function_from_data_flow(graph,
	{
		n1->input(0),
		npass->input(0)
	},
	{
		n2->output(0)
	});

	BLI::RefCounted<FN::Function> *fn_ref = fn.refcounter();
	fn_ref->incref();
	return wrap(fn_ref);
}