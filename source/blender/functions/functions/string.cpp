#include "FN_types.hpp"
#include "FN_functions.hpp"
#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"
#include "BLI_lazy_init.hpp"
#include "BLI_math.h"

namespace FN {
namespace Functions {

using namespace Types;

class StringLength : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    MyString str = fn_in.relocate_out<MyString>(0);
    int length = str.size();
    fn_out.set<int32_t>(0, length);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_string_length)
{
  FunctionBuilder builder;
  builder.add_input("String", TYPE_string);
  builder.add_output("Length", TYPE_int32);

  auto fn = builder.build("String Length");
  fn->add_body<StringLength>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
