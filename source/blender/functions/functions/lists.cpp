#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN {
namespace Functions {

using namespace Types;

template<typename T> class CreateEmptyList : public TupleCallBody {
  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = SharedList<T>::New();
    fn_out.move_in(0, list);
  }
};

template<typename T>
SharedFunction build_create_empty_list_function(SharedType &base_type, SharedType &list_type)
{
  std::string name = "Create Empty " + base_type->name() + " List";
  auto fn = SharedFunction::New(name,
                                Signature({},
                                          {
                                              OutputParameter("List", list_type),
                                          }));
  fn->add_body(new CreateEmptyList<T>());
  return fn;
}

template<typename T> class CreateSingleElementList : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = SharedList<T>::New();
    T value = fn_in.relocate_out<T>(0);
    list->append(value);
    fn_out.move_in(0, list);
  }
};

template<typename T>
SharedFunction build_create_single_element_list_function(SharedType &base_type,
                                                         SharedType &list_type)
{
  std::string name = "Create " + base_type->name() + " List from Value";
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("Value", base_type),
                                    },
                                    {
                                        OutputParameter("List", list_type),
                                    }));
  fn->add_body(new CreateSingleElementList<T>());
  return fn;
}

template<typename T> class AppendToList : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = fn_in.relocate_out<SharedList<T>>(0);
    T value = fn_in.relocate_out<T>(1);

    list = list->get_mutable();
    list->append(value);

    fn_out.move_in(0, list);
  }
};

template<typename T>
SharedFunction build_append_function(SharedType &base_type, SharedType &list_type)
{
  std::string name = "Append " + base_type->name();
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("List", list_type),
                                        InputParameter("Value", base_type),
                                    },
                                    {
                                        OutputParameter("List", list_type),
                                    }));
  fn->add_body(new AppendToList<T>());
  return fn;
}

template<typename T> class GetListElement : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = fn_in.get_ref<SharedList<T>>(0);
    int32_t index = fn_in.get<int32_t>(1);

    if (index >= 0 && index < list->size()) {
      const List<T> *list_ = list.ptr();
      T value = (*list_)[index];
      fn_out.move_in(0, value);
    }
    else {
      T fallback = fn_in.relocate_out<T>(2);
      fn_out.move_in(0, fallback);
    }
  }
};

template<typename T>
SharedFunction build_get_element_function(SharedType &base_type, SharedType &list_type)
{
  std::string name = "Get " + base_type->name() + " List Element";
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("List", list_type),
                                        InputParameter("Index", GET_TYPE_int32()),
                                        InputParameter("Fallback", base_type),
                                    },
                                    {
                                        OutputParameter("Element", base_type),
                                    }));
  fn->add_body(new GetListElement<T>());
  return fn;
}

template<typename T> class CombineLists : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list1 = fn_in.relocate_out<SharedList<T>>(0);
    auto list2 = fn_in.relocate_out<SharedList<T>>(1);

    list1 = list1->get_mutable();
    list1->extend(list2.ptr());

    fn_out.move_in(0, list1);
  }
};

template<typename T>
SharedFunction build_combine_lists_function(SharedType &base_type, SharedType &list_type)
{
  std::string name = "Combine " + base_type->name() + " Lists";
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("List 1", list_type),
                                        InputParameter("List 2", list_type),
                                    },
                                    {
                                        OutputParameter("List", list_type),
                                    }));
  fn->add_body(new CombineLists<T>());
  return fn;
}

template<typename T> class ListLength : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = fn_in.relocate_out<SharedList<T>>(0);
    uint length = list->size();
    fn_out.set<uint>(0, length);
  }
};

template<typename T>
SharedFunction build_list_length_function(SharedType &base_type, SharedType &list_type)
{
  std::string name = base_type->name() + " List Length";
  auto fn = SharedFunction::New(name,
                                Signature(
                                    {
                                        InputParameter("List", list_type),
                                    },
                                    {
                                        OutputParameter("Length", GET_TYPE_int32()),
                                    }));
  fn->add_body(new ListLength<T>());
  return fn;
}

/* Build List Functions
   *************************************/

struct ListFunctions {
  FunctionPerType m_create_empty;
  FunctionPerType m_from_element;
  FunctionPerType m_append;
  FunctionPerType m_get_element;
  FunctionPerType m_combine;
  FunctionPerType m_length;
};

template<typename T>
void insert_list_functions_for_type(ListFunctions &functions,
                                    SharedType &base_type,
                                    SharedType &list_type)
{
  functions.m_create_empty.add(base_type,
                               build_create_empty_list_function<T>(base_type, list_type));
  functions.m_from_element.add(base_type,
                               build_create_single_element_list_function<T>(base_type, list_type));
  functions.m_append.add(base_type, build_append_function<T>(base_type, list_type));
  functions.m_get_element.add(base_type, build_get_element_function<T>(base_type, list_type));
  functions.m_combine.add(base_type, build_combine_lists_function<T>(base_type, list_type));
  functions.m_length.add(base_type, build_list_length_function<T>(base_type, list_type));
}

LAZY_INIT_REF_STATIC__NO_ARG(ListFunctions, get_list_functions)
{
  ListFunctions functions;
  insert_list_functions_for_type<float>(functions, GET_TYPE_float(), GET_TYPE_float_list());
  insert_list_functions_for_type<Vector>(functions, GET_TYPE_fvec3(), GET_TYPE_fvec3_list());
  insert_list_functions_for_type<int32_t>(functions, GET_TYPE_int32(), GET_TYPE_int32_list());
  return functions;
}

/* Access List Functions
   *************************************/

SharedFunction &GET_FN_empty_list(SharedType &base_type)
{
  FunctionPerType &functions = get_list_functions().m_create_empty;
  BLI_assert(functions.contains(base_type));
  return functions.lookup_ref(base_type);
}

SharedFunction &GET_FN_list_from_element(SharedType &base_type)
{
  FunctionPerType &functions = get_list_functions().m_from_element;
  BLI_assert(functions.contains(base_type));
  return functions.lookup_ref(base_type);
}

SharedFunction &GET_FN_append_to_list(SharedType &base_type)
{
  FunctionPerType &functions = get_list_functions().m_append;
  BLI_assert(functions.contains(base_type));
  return functions.lookup_ref(base_type);
}

SharedFunction &GET_FN_get_list_element(SharedType &base_type)
{
  FunctionPerType &functions = get_list_functions().m_get_element;
  BLI_assert(functions.contains(base_type));
  return functions.lookup_ref(base_type);
}

SharedFunction &GET_FN_combine_lists(SharedType &base_type)
{
  FunctionPerType &functions = get_list_functions().m_combine;
  BLI_assert(functions.contains(base_type));
  return functions.lookup_ref(base_type);
}

SharedFunction &GET_FN_list_length(SharedType &base_type)
{
  FunctionPerType &functions = get_list_functions().m_length;
  BLI_assert(functions.contains(base_type));
  return functions.lookup_ref(base_type);
}

SharedType &get_list_type(SharedType &base_type)
{
  SharedFunction &fn = GET_FN_append_to_list(base_type);
  return fn->signature().inputs()[0].type();
}

}  // namespace Functions
}  // namespace FN
