#include "FN_tuple_call.hpp"

using namespace FN;

void FN_tuple_call_invoke(FnTupleCallBody body_c,
                          FnTuple fn_in_c,
                          FnTuple fn_out_c,
                          const char *caller_info)
{
  Tuple &fn_in = *unwrap(fn_in_c);
  Tuple &fn_out = *unwrap(fn_out_c);
  TupleCallBody *body = unwrap(body_c);
  BLI_assert(fn_in.all_initialized());

  /* setup stack */
  ExecutionStack stack;
  TextStackFrame caller_frame(caller_info);
  stack.push(&caller_frame);

  ExecutionContext ctx(stack);
  body->call__setup_stack(fn_in, fn_out, ctx);
  BLI_assert(fn_out.all_initialized());
}

FnTupleCallBody FN_tuple_call_get(FnFunction fn_c)
{
  return wrap(&unwrap(fn_c)->body<TupleCallBody>());
}

uint fn_tuple_stack_prepare_size(FnTupleCallBody body_c)
{
  TupleCallBody *body = unwrap(body_c);
  return body->meta_in().size_of_full_tuple() + body->meta_out().size_of_full_tuple();
}

void fn_tuple_prepare_stack(FnTupleCallBody body_c,
                            void *buffer,
                            FnTuple *fn_in_c,
                            FnTuple *fn_out_c)
{
  TupleCallBody *body = unwrap(body_c);
  char *buf = (char *)buffer;
  char *buf_in = buf + 0;
  char *buf_out = buf + body->meta_in().size_of_full_tuple();
  Tuple::ConstructInBuffer(body->meta_in(), buf_in);
  Tuple::ConstructInBuffer(body->meta_out(), buf_out);
  *fn_in_c = wrap((Tuple *)buf_in);
  *fn_out_c = wrap((Tuple *)buf_out);
}
