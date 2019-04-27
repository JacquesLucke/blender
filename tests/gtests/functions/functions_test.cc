#include "testing/testing.h"
#include "FN_all.hpp"

using namespace FN;

TEST(functions_impl, MultiplyFloats)
{
  auto fn = Functions::GET_FN_multiply_floats();
  auto *body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

  fn_in.set<float>(0, 4);
  fn_in.set<float>(1, 20);

  body->call__setup_execution_context(fn_in, fn_out);

  EXPECT_EQ(fn_out.get<float>(0), 80);
}
