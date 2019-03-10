#include "FN-C.h"
#include "FN_all.hpp"

#include "BLI_timeit.hpp"

#include <iostream>

using namespace BLI;

using namespace FN;
using namespace FN::Types;
using namespace FN::DataFlowNodes;

#define WRAPPERS(T1, T2) \
	inline T1 unwrap(T2 value) { return (T1)value; } \
	inline T2 wrap(T1 value) { return (T2)value; }


WRAPPERS(Function *, FnFunction);
WRAPPERS(Type *, FnType);
WRAPPERS(Tuple *, FnTuple);
WRAPPERS(TupleCallBody *, FnTupleCallBody);

static void playground()
{
	SharedFunction fn = Functions::append_float();

	Tuple fn_in(fn->signature().input_types());
	Tuple fn_out(fn->signature().output_types());

	auto list = SharedFloatList::New();

	BLI_assert(list->users() == 1);
	fn_in.copy_in(0, list);
	BLI_assert(list->users() == 2);

	fn_in.set<float>(1, 42.0f);

	BLI_assert(list->users() == 2);
	fn->body<TupleCallBody>()->call(fn_in, fn_out);
	BLI_assert(list->users() == 1);

	auto new_list = fn_out.relocate_out<SharedFloatList>(0);
	BLI_assert(new_list->users() == 1);
}

void FN_initialize()
{
	initialize_llvm();
	playground();
}

void FN_tuple_call_invoke(FnTupleCallBody fn_call, FnTuple fn_in, FnTuple fn_out)
{
	Tuple &fn_in_ = *unwrap(fn_in);
	Tuple &fn_out_ = *unwrap(fn_out);

	BLI_assert(fn_in_.all_initialized());
	unwrap(fn_call)->call(fn_in_, fn_out_);
	BLI_assert(fn_out_.all_initialized());
}

FnTupleCallBody FN_tuple_call_get(FnFunction fn)
{
	return wrap(unwrap(fn)->body<TupleCallBody>());
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
	return unwrap(fn)->signature().inputs().size();
}

uint FN_output_amount(FnFunction fn)
{
	return unwrap(fn)->signature().outputs().size();
}

bool FN_input_has_type(FnFunction fn, uint index, FnType type)
{
	Type *type1 = unwrap(fn)->signature().inputs()[index].type().ptr();
	Type *type2 = unwrap(type);
	return type1 == type2;
}

bool FN_output_has_type(FnFunction fn, uint index, FnType type)
{
	Type *type1 = unwrap(fn)->signature().outputs()[index].type().ptr();
	Type *type2 = unwrap(type);
	return type1 == type2;
}

void FN_function_print(FnFunction fn)
{
	Function *function = unwrap(fn);
	function->print();
}


FnTuple FN_tuple_for_input(FnTupleCallBody body)
{
	auto tuple = new Tuple(unwrap(body)->meta_in());
	return wrap(tuple);
}

FnTuple FN_tuple_for_output(FnTupleCallBody body)
{
	auto tuple = new Tuple(unwrap(body)->meta_out());
	return wrap(tuple);
}

void FN_tuple_free(FnTuple tuple)
{
	delete unwrap(tuple);
}

uint fn_tuple_stack_prepare_size(FnTupleCallBody body_)
{
	TupleCallBody *body = unwrap(body_);
	return body->meta_in()->total_stack_size() + body->meta_out()->total_stack_size();
}

static Tuple *init_tuple(SharedTupleMeta &meta, void *buffer)
{
	char *buf = (char *)buffer;
	char *tuple_buf = buf + 0;
	char *data_buf = buf + sizeof(Tuple);
	char *init_buf = data_buf + meta->total_data_size();
	return new(tuple_buf) Tuple(meta, data_buf, (bool *)init_buf, false);
}

void fn_tuple_prepare_stack(
	FnTupleCallBody body_,
	void *buffer,
	FnTuple *fn_in_,
	FnTuple *fn_out_)
{
	TupleCallBody *body = unwrap(body_);
	char *buf = (char *)buffer;
	char *buf_in = buf + 0;
	char *buf_out = buf + body->meta_in()->total_stack_size();
	Tuple *fn_in = init_tuple(body->meta_in(), buf_in);
	Tuple *fn_out = init_tuple(body->meta_out(), buf_out);
	*fn_in_ = wrap(fn_in);
	*fn_out_ = wrap(fn_out);
}

void fn_tuple_destruct(FnTuple tuple)
{
	unwrap(tuple)->~Tuple();
}

void FN_tuple_set_float(FnTuple tuple, uint index, float value)
{
	unwrap(tuple)->set<float>(index, value);
}

float FN_tuple_get_float(FnTuple tuple, uint index)
{
	return unwrap(tuple)->get<float>(index);
}

void FN_tuple_set_int32(FnTuple tuple, uint index, int32_t value)
{
	unwrap(tuple)->set<int32_t>(index, value);
}

int32_t FN_tuple_get_int32(FnTuple tuple, uint index)
{
	return unwrap(tuple)->get<int32_t>(index);
}

using Types::Vector;

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
	return unwrap(type)->name().c_str();
}

void FN_type_free(FnType type)
{
	unwrap(type)->decref();
}

static FnType get_type_with_increased_refcount(const SharedType &type)
{
	Type *typeref = type.ptr();
	typeref->incref();
	return wrap(typeref);
}

#define SIMPLE_TYPE_GETTER(name) \
	FnType FN_type_get_##name() \
	{ return get_type_with_increased_refcount(Types::get_##name##_type()); } \
	FnType FN_type_borrow_##name() \
	{ return wrap(Types::get_##name##_type().ptr()); }

SIMPLE_TYPE_GETTER(float);
SIMPLE_TYPE_GETTER(int32);
SIMPLE_TYPE_GETTER(fvec3);

FnFunction FN_tree_to_function(bNodeTree *btree)
{
	TIMEIT("Tree to function");
	BLI_assert(btree);
	auto fn_opt = DataFlowNodes::generate_function(btree);
	if (!fn_opt.has_value()) {
		return nullptr;
	}

	Function *fn_ptr = fn_opt.value().ptr();
	fn_ptr->incref();
	return wrap(fn_ptr);
}

FnFunction FN_function_get_with_signature(
	bNodeTree *btree, FnType *inputs, FnType *outputs)
{
	if (btree == NULL) {
		return NULL;
	}

	FnFunction fn = FN_tree_to_function(btree);
	if (fn == NULL) {
		return NULL;
	}
	else if (FN_function_has_signature(fn, inputs, outputs)) {
		return fn;
	}
	else {
		FN_function_free(fn);
		return NULL;
	}
}

void FN_function_update_dependencies(
	FnFunction fn,
	struct DepsNodeHandle *deps_node)
{
	Function *fn_ = unwrap(fn);
	const DependenciesBody *body = fn_->body<DependenciesBody>();
	if (body) {
		Dependencies dependencies;
		body->dependencies(dependencies);
		dependencies.update_depsgraph(deps_node);
	}
}