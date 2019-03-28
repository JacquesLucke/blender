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



/************* Generic *****************/

void FN_initialize()
{
	initialize_llvm();
}



/************** Core ****************/

WRAPPERS(Function *, FnFunction);
WRAPPERS(Type *, FnType);

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



/**************** Types ******************/

WRAPPERS(List<float> *, FnFloatList);
WRAPPERS(List<Vector> *, FnFVec3List);

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
SIMPLE_TYPE_GETTER(float_list);
SIMPLE_TYPE_GETTER(fvec3_list);

#define LIST_WRAPPER(name, ptr_type, list_type) \
	uint FN_list_size_##name(list_type list) \
	{ return unwrap(list)->size(); } \
	ptr_type FN_list_data_##name(list_type list) \
	{ return (ptr_type)unwrap(list)->data_ptr(); } \
	void FN_list_free_##name(list_type list) \
	{ unwrap(list)->remove_user(); }

LIST_WRAPPER(float, float *, FnFloatList);
LIST_WRAPPER(fvec3, float *, FnFVec3List);



/***************** Tuple Call ******************/

WRAPPERS(Tuple *, FnTuple);
WRAPPERS(TupleCallBody *, FnTupleCallBody);

void FN_tuple_call_invoke(
	FnTupleCallBody body,
	FnTuple fn_in,
	FnTuple fn_out,
	const char *caller_info)
{
	Tuple &fn_in_ = *unwrap(fn_in);
	Tuple &fn_out_ = *unwrap(fn_out);
	TupleCallBody *body_ = unwrap(body);
	BLI_assert(fn_in_.all_initialized());

	/* setup stack */
	ExecutionStack stack;
	TextStackFrame caller_frame(caller_info);
	stack.push(&caller_frame);
	TextStackFrame function_frame(body_->owner()->name().c_str());
	stack.push(&function_frame);

	ExecutionContext ctx(stack);
	body_->call(fn_in_, fn_out_, ctx);
	BLI_assert(fn_out_.all_initialized());
}

FnTupleCallBody FN_tuple_call_get(FnFunction fn)
{
	return wrap(unwrap(fn)->body<TupleCallBody>());
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
	return body->meta_in()->total_size() + body->meta_out()->total_size();
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
	char *buf_out = buf + body->meta_in()->total_size();
	Tuple::NewInBuffer(body->meta_in(), buf_in);
	Tuple::NewInBuffer(body->meta_out(), buf_out);
	*fn_in_ = wrap((Tuple *)buf_in);
	*fn_out_ = wrap((Tuple *)buf_out);
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

void FN_tuple_set_fvec3(FnTuple tuple, uint index, float value[3])
{
	unwrap(tuple)->set<Vector>(index, *(Vector *)value);
}

void FN_tuple_get_fvec3(FnTuple tuple, uint index, float dst[3])
{
	*(Vector *)dst = unwrap(tuple)->get<Vector>(index);
}

FnFloatList FN_tuple_relocate_out_float_list(FnTuple tuple, uint index)
{
	auto list = unwrap(tuple)->relocate_out<SharedFloatList>(index);
	return wrap(list.extract_ptr());
}

FnFVec3List FN_tuple_relocate_out_fvec3_list(FnTuple tuple, uint index)
{
	auto list = unwrap(tuple)->relocate_out<SharedFVec3List>(index);
	return wrap(list.extract_ptr());
}


/**************** Dependencies *******************/

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



/****************** Data Flow Nodes *****************/

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
