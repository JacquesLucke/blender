#include "FN_tuple_call.hpp"

using namespace FN;

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