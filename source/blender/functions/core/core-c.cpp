#include "FN_core.hpp"

using namespace FN;

void FN_function_free(FnFunction fn_c)
{
  delete unwrap(fn_c);
}

bool FN_function_has_signature(FnFunction fn_c, FnType *inputs_c, FnType *outputs_c)
{
  uint input_amount;
  uint output_amount;
  for (input_amount = 0; inputs_c[input_amount]; input_amount++) {
  }
  for (output_amount = 0; outputs_c[output_amount]; output_amount++) {
  }

  if (FN_input_amount(fn_c) != input_amount)
    return false;
  if (FN_output_amount(fn_c) != output_amount)
    return false;

  for (uint i = 0; i < input_amount; i++) {
    if (!FN_input_has_type(fn_c, i, inputs_c[i]))
      return false;
  }
  for (uint i = 0; i < output_amount; i++) {
    if (!FN_output_has_type(fn_c, i, outputs_c[i]))
      return false;
  }
  return true;
}

uint FN_input_amount(FnFunction fn_c)
{
  return unwrap(fn_c)->input_amount();
}

uint FN_output_amount(FnFunction fn_c)
{
  return unwrap(fn_c)->output_amount();
}

bool FN_input_has_type(FnFunction fn_c, uint index, FnType type_c)
{
  Type *type1 = unwrap(fn_c)->input_type(index);
  Type *type2 = unwrap(type_c);
  return type1 == type2;
}

bool FN_output_has_type(FnFunction fn_c, uint index, FnType type_c)
{
  Type *type1 = unwrap(fn_c)->output_type(index);
  Type *type2 = unwrap(type_c);
  return type1 == type2;
}

void FN_function_print(FnFunction fn_c)
{
  Function *fn = unwrap(fn_c);
  fn->print();
}

const char *FN_type_name(FnType type)
{
  return unwrap(type)->name().data();
}
