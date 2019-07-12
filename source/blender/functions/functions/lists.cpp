#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"

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
  FunctionBuilder builder;
  builder.add_output("List", list_type);

  std::string name = "Create Empty " + base_type->name() + " List";
  auto fn = builder.build(name);
  fn->add_body<CreateEmptyList<T>>();
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

class CreateSingleElementListDependencies : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    builder.pass_ids_through(0, 0);
  }
};

template<typename T>
SharedFunction build_create_single_element_list_function(SharedType &base_type,
                                                         SharedType &list_type)
{
  FunctionBuilder builder;
  builder.add_input("Value", base_type);
  builder.add_output("List", list_type);

  std::string name = "Create " + base_type->name() + " List from Value";
  auto fn = builder.build(name);
  fn->add_body<CreateSingleElementList<T>>();
  if (base_type == GET_TYPE_object()) {
    fn->add_body<CreateSingleElementListDependencies>();
  }
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

class AppendToListDependencies : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    builder.pass_ids_through(0, 0);
    builder.pass_ids_through(1, 0);
  }
};

template<typename T>
SharedFunction build_append_function(SharedType &base_type, SharedType &list_type)
{
  FunctionBuilder builder;
  builder.add_input("List", list_type);
  builder.add_input("Value", base_type);
  builder.add_output("List", list_type);

  std::string name = "Append " + base_type->name();
  auto fn = builder.build(name);
  fn->add_body<AppendToList<T>>();
  if (base_type == GET_TYPE_object()) {
    fn->add_body<AppendToListDependencies>();
  }
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

class GetListElementDependencies : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    builder.pass_ids_through(0, 0);
    builder.pass_ids_through(2, 0);
  }
};

template<typename T>
SharedFunction build_get_element_function(SharedType &base_type, SharedType &list_type)
{
  FunctionBuilder builder;
  builder.add_input("List", list_type);
  builder.add_input("Index", GET_TYPE_int32());
  builder.add_input("Fallback", base_type);
  builder.add_output("Element", base_type);

  std::string name = "Get " + base_type->name() + " List Element";
  auto fn = builder.build(name);
  fn->add_body<GetListElement<T>>();
  if (base_type == GET_TYPE_object()) {
    fn->add_body<GetListElementDependencies>();
  }
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

class CombineListsDependencies : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    builder.pass_ids_through(0, 0);
    builder.pass_ids_through(1, 0);
  }
};

template<typename T>
SharedFunction build_combine_lists_function(SharedType &base_type, SharedType &list_type)
{
  FunctionBuilder builder;
  builder.add_input("List 1", list_type);
  builder.add_input("List 2", list_type);
  builder.add_output("List", list_type);

  std::string name = "Combine " + base_type->name() + " Lists";
  auto fn = builder.build(name);
  fn->add_body<CombineLists<T>>();
  if (base_type == GET_TYPE_object()) {
    fn->add_body<CombineListsDependencies>();
  }
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
  FunctionBuilder builder;
  builder.add_input("List", list_type);
  builder.add_output("Length", GET_TYPE_int32());

  std::string name = base_type->name() + " List Length";
  auto fn = builder.build(name);
  fn->add_body<ListLength<T>>();
  return fn;
}

/* C Functions for List access
 **************************************/

template<typename T> uint get_list_length(void *list)
{
  const List<T> *list_ = (Types::List<T> *)list;
  return list_->size();
}

template<typename T> void *get_value_ptr(void *list)
{
  const List<T> *list_ = (Types::List<T> *)list;
  return (void *)list_->data_ptr();
}

template<typename T> void *new_list_with_prepared_memory(uint length)
{
  auto *list = new Types::List<T>(length);
  return (void *)list;
}

/* Build List Functions
 *************************************/

using FunctionPerType = SmallMap<SharedType, SharedFunction>;

struct ListFunctions {
  FunctionPerType m_create_empty;
  FunctionPerType m_from_element;
  FunctionPerType m_append;
  FunctionPerType m_get_element;
  FunctionPerType m_combine;
  FunctionPerType m_length;

  SmallMap<SharedType, GetListLength> m_c_length;
  SmallMap<SharedType, GetListDataPtr> m_c_data_ptr;
  SmallMap<SharedType, NewListWithAllocatedBuffer> m_c_new_allocated;
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

  functions.m_c_length.add(base_type, get_list_length<T>);
  functions.m_c_data_ptr.add(base_type, get_value_ptr<T>);
  functions.m_c_new_allocated.add(base_type, new_list_with_prepared_memory<T>);
}

BLI_LAZY_INIT_STATIC(ListFunctions, get_list_functions)
{
  ListFunctions functions;
  insert_list_functions_for_type<float>(functions, GET_TYPE_float(), GET_TYPE_float_list());
  insert_list_functions_for_type<float3>(functions, GET_TYPE_float3(), GET_TYPE_float3_list());
  insert_list_functions_for_type<int32_t>(functions, GET_TYPE_int32(), GET_TYPE_int32_list());
  insert_list_functions_for_type<bool>(functions, GET_TYPE_bool(), GET_TYPE_bool_list());
  insert_list_functions_for_type<Object *>(functions, GET_TYPE_object(), GET_TYPE_object_list());
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
  return fn->input_type(0);
}

GetListLength GET_C_FN_list_length(SharedType &base_type)
{
  return get_list_functions().m_c_length.lookup(base_type);
}
GetListDataPtr GET_C_FN_list_data_ptr(SharedType &base_type)
{
  return get_list_functions().m_c_data_ptr.lookup(base_type);
}
NewListWithAllocatedBuffer GET_C_FN_new_list_with_allocated_buffer(SharedType &base_type)
{
  return get_list_functions().m_c_new_allocated.lookup(base_type);
}

}  // namespace Functions
}  // namespace FN
