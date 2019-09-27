#include "BLI_lazy_init_cxx.h"

#include "FN_tuple_call.hpp"
#include "FN_types.hpp"

#include "boolean.hpp"

namespace FN {
namespace Functions {

using namespace Types;

static std::unique_ptr<Function> get_boolean_function__one_input(StringRef name)
{
  FunctionBuilder builder;
  builder.add_input("Value", TYPE_bool);
  builder.add_output("Result", TYPE_bool);
  return builder.build(name);
}

static std::unique_ptr<Function> get_boolean_function__two_inputs(StringRef name)
{
  FunctionBuilder builder;
  builder.add_input("A", TYPE_bool);
  builder.add_input("B", TYPE_bool);
  builder.add_output("Result", TYPE_bool);
  return builder.build(name);
}

class AndBoolean : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const
  {
    bool a = fn_in.get<bool>(0);
    bool b = fn_in.get<bool>(1);
    bool result = a && b;
    fn_out.set<bool>(0, result);
  }
};

BLI_LAZY_INIT_REF(Function, GET_FN_and)
{
  auto fn = get_boolean_function__two_inputs("And");
  fn->add_body<AndBoolean>();
  return fn;
}

class OrBoolean : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const
  {
    bool a = fn_in.get<bool>(0);
    bool b = fn_in.get<bool>(1);
    bool result = a || b;
    fn_out.set<bool>(0, result);
  }
};

BLI_LAZY_INIT_REF(Function, GET_FN_or)
{
  auto fn = get_boolean_function__two_inputs("Or");
  fn->add_body<OrBoolean>();
  return fn;
}

class NotBoolean : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const
  {
    bool value = fn_in.get<bool>(0);
    bool result = !value;
    fn_out.set<bool>(0, result);
  }
};

BLI_LAZY_INIT_REF(Function, GET_FN_not)
{
  auto fn = get_boolean_function__one_input("Not");
  fn->add_body<NotBoolean>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
