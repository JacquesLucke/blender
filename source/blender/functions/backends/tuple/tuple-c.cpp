#include "FN_tuple.hpp"

void FN_tuple_free(FnTuple tuple_c)
{
  delete unwrap(tuple_c);
}

void fn_tuple_destruct(FnTuple tuple_c)
{
  unwrap(tuple_c)->~Tuple();
}
