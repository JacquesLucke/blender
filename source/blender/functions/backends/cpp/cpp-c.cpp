#include "FN_cpp.hpp"

using namespace FN;

void FN_tuple_free(FnTuple tuple_c)
{
  Tuple *tuple = unwrap(tuple_c);
  delete tuple;
}

void fn_tuple_destruct(FnTuple tuple_c)
{
  Tuple *tuple = unwrap(tuple_c);
  tuple->~Tuple();
}

uint FN_list_size(FnList list_c)
{
  List *list = unwrap(list_c);
  return list->size();
}

void *FN_list_storage(FnList list_c)
{
  List *list = unwrap(list_c);
  return list->storage();
}

void FN_list_free(FnList list_c)
{
  List *list = unwrap(list_c);
  list->decref();
}
