#include "testing/testing.h"
#include "FN_all.hpp"

using namespace FN;

#define PREPARE_TUPLE_CALL_TEST(FN) \
  auto *body = FN->body<TupleCallBody>(); \
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);

class functions_impl : public testing::Test {
 protected:
  void SetUp() override
  {
    FN_initialize();
  }
};

TEST_F(functions_impl, MultiplyFloats)
{
  PREPARE_TUPLE_CALL_TEST(Functions::GET_FN_multiply_floats());

  fn_in.set<float>(0, 4);
  fn_in.set<float>(1, 20);

  body->call__setup_execution_context(fn_in, fn_out);

  EXPECT_EQ(fn_out.get<float>(0), 80);
}

TEST_F(functions_impl, FloatRange)
{
  PREPARE_TUPLE_CALL_TEST(Functions::GET_FN_float_range());

  fn_in.set<int>(0, 4);
  fn_in.set<float>(1, 2);
  fn_in.set<float>(2, 7);

  body->call__setup_execution_context(fn_in, fn_out);

  auto list = fn_out.relocate_out<Types::SharedFloatList>(0);
  EXPECT_EQ(list->size(), 4);

  float *ptr = list->data_ptr();
  EXPECT_EQ(ptr[0], 2);
  EXPECT_EQ(ptr[1], 9);
  EXPECT_EQ(ptr[2], 16);
  EXPECT_EQ(ptr[3], 23);
}

TEST_F(functions_impl, AddFloats)
{
  auto fn = Functions::GET_FN_add_floats();
  derive_TupleCallBody_from_LLVMBuildIRBody(fn, *(new llvm::LLVMContext()));
  PREPARE_TUPLE_CALL_TEST(fn);

  fn_in.set<float>(0, 3.5f);
  fn_in.set<float>(1, 1.5f);

  body->call__setup_execution_context(fn_in, fn_out);

  EXPECT_EQ(fn_out.get<float>(0), 5.0f);
}
