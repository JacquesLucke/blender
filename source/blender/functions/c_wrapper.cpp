#include "FN_functions.h"
#include "FN_functions.hpp"

#include "nodes/nodes.hpp"
#include "nodes/graph_generation.hpp"

#include "BLI_timeit.hpp"

#include <iostream>

#define WRAPPERS(T1, T2) \
	inline T1 unwrap(T2 value) { return (T1)value; } \
	inline T2 wrap(T1 value) { return (T2)value; }


WRAPPERS(BLI::RefCounted<FN::Function> *, FnFunction);
WRAPPERS(BLI::RefCounted<FN::Type> *, FnType);
WRAPPERS(FN::Tuple *, FnTuple);
WRAPPERS(const FN::TupleCallBody *, FnCallable);


void FN_initialize()
{
	FN::Nodes::initialize();
}

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


bool FN_function_has_signature(FnFunction fn, FnType *inputs, FnType *outputs)
{
	uint input_amount;
	uint output_amount;
	for (input_amount = 0; inputs[input_amount]; input_amount++) {}
	for (output_amount = 0; outputs[output_amount]; output_amount++) {}

	if (FN_input_amount(fn) != input_amount) return false;
	if (FN_output_amount(fn) != output_amount) return false;

	for (uint i = 0; i < input_amount; i++) {
		if (!FN_input_has_type(fn, i, inputs[i])) return false;
	}
	for (uint i = 0; i < output_amount; i++) {
		if (!FN_output_has_type(fn, i, outputs[i])) return false;
	}
	return true;
}

uint FN_input_amount(FnFunction fn)
{
	return unwrap(fn)->ptr()->signature().inputs().size();
}

uint FN_output_amount(FnFunction fn)
{
	return unwrap(fn)->ptr()->signature().outputs().size();
}

bool FN_input_has_type(FnFunction fn, uint index, FnType type)
{
	FN::Type *type1 = unwrap(fn)->ptr()->signature().inputs()[index].type().refcounter()->ptr();
	FN::Type *type2 = unwrap(type)->ptr();
	return type1 == type2;
}

bool FN_output_has_type(FnFunction fn, uint index, FnType type)
{
	FN::Type *type1 = unwrap(fn)->ptr()->signature().outputs()[index].type().refcounter()->ptr();
	FN::Type *type2 = unwrap(type)->ptr();
	return type1 == type2;
}

void FN_function_print(FnFunction fn)
{
	FN::Function *function = unwrap(fn)->ptr();
	function->print();
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

using FN::Types::Vector;

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

FnFunction FN_tree_to_function(bNodeTree *btree)
{
	TIMEIT("Tree to function");
	auto fgraph = FN::Nodes::btree_to_graph(btree);
	//std::cout << fgraph.graph()->to_dot() << std::endl;

	auto fn = FN::SharedFunction::New("Function from Node Tree", fgraph.signature());
	fn->add_body(FN::function_graph_to_callable(fgraph));

	BLI::RefCounted<FN::Function> *fn_ref = fn.refcounter();
	fn_ref->incref();
	return wrap(fn_ref);
}

void FN_function_update_dependencies(
	FnFunction fn,
	struct DepsNodeHandle *deps_node)
{
	BLI::RefCounted<FN::Function> *fn_ref = unwrap(fn);
	FN::Dependencies dependencies;
	fn_ref->ptr()->body<FN::TupleCallBody>()->dependencies(dependencies);
	dependencies.update_depsgraph(deps_node);
}