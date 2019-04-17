#include "FN_core.hpp"

using namespace FN;

void FN_function_free(FnFunction fn)
{
  unwrap(fn)->decref();
}

bool FN_function_has_signature(FnFunction fn, FnType *inputs, FnType *outputs)
{
  uint input_amount;
  uint output_amount;
  for (input_amount = 0; inputs[input_amount]; input_amount++) {
  }
  for (output_amount = 0; outputs[output_amount]; output_amount++) {
  }

  if (FN_input_amount(fn) != input_amount)
    return false;
  if (FN_output_amount(fn) != output_amount)
    return false;

  for (uint i = 0; i < input_amount; i++) {
    if (!FN_input_has_type(fn, i, inputs[i]))
      return false;
  }
  for (uint i = 0; i < output_amount; i++) {
    if (!FN_output_has_type(fn, i, outputs[i]))
      return false;
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
