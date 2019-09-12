#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"

#include "BLI_lazy_init_cxx.h"

namespace FN {
namespace Functions {

using namespace Types;

class CreateEmptyList : public TupleCallBody {
 private:
  Type *m_base_type;

 public:
  CreateEmptyList(Type *base_type) : m_base_type(std::move(base_type))
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = SharedList::New(m_base_type);
    fn_out.move_in(0, list);
  }
};

static SharedFunction build_create_empty_list_function(Type *base_type, Type *list_type)
{
  FunctionBuilder builder;
  builder.add_output("List", list_type);

  std::string name = "Create Empty " + base_type->name() + " List";
  auto fn = builder.build(name);
  fn->add_body<CreateEmptyList>(base_type);
  return fn;
}

class CreateSingleElementList : public TupleCallBody {
 private:
  Type *m_base_type;

 public:
  CreateSingleElementList(Type *base_type) : m_base_type(std::move(base_type))
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = SharedList::New(m_base_type);
    list->append__dynamic_relocate_from_tuple(fn_in, 0);
    fn_out.move_in(0, list);
  }
};

class CreateSingleElementListDependencies : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    builder.pass_ids_through(0, 0);
  }
};

static SharedFunction build_create_single_element_list_function(Type *base_type, Type *list_type)
{
  FunctionBuilder builder;
  builder.add_input("Value", base_type);
  builder.add_output("List", list_type);

  std::string name = "Create " + base_type->name() + " List from Value";
  auto fn = builder.build(name);
  fn->add_body<CreateSingleElementList>(base_type);
  if (base_type == TYPE_object) {
    fn->add_body<CreateSingleElementListDependencies>();
  }
  return fn;
}

class AppendToList : public TupleCallBody {
 private:
  Type *m_base_type;

 public:
  AppendToList(Type *base_type) : m_base_type(std::move(base_type))
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = fn_in.relocate_out<SharedList>(0);
    list = list->get_mutable();
    list->append__dynamic_relocate_from_tuple(fn_in, 1);
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

static SharedFunction build_append_function(Type *base_type, Type *list_type)
{
  FunctionBuilder builder;
  builder.add_input("List", list_type);
  builder.add_input("Value", base_type);
  builder.add_output("List", list_type);

  std::string name = "Append " + base_type->name();
  auto fn = builder.build(name);
  fn->add_body<AppendToList>(base_type);
  if (base_type == TYPE_object) {
    fn->add_body<AppendToListDependencies>();
  }
  return fn;
}

class GetListElement : public TupleCallBody {
 private:
  Type *m_base_type;

 public:
  GetListElement(Type *base_type) : m_base_type(std::move(base_type))
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto &list = fn_in.get_ref<SharedList>(0);
    int32_t index = fn_in.get<int32_t>(1);

    if (index >= 0 && index < list->size()) {
      list->get__dynamic_copy_to_tuple(index, fn_out, 0);
    }
    else {
      Tuple::relocate_element(fn_in, 2, fn_out, 0);
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

static SharedFunction build_get_element_function(Type *base_type, Type *list_type)
{
  FunctionBuilder builder;
  builder.add_input("List", list_type);
  builder.add_input("Index", TYPE_int32);
  builder.add_input("Fallback", base_type);
  builder.add_output("Element", base_type);

  std::string name = "Get " + base_type->name() + " List Element";
  auto fn = builder.build(name);
  fn->add_body<GetListElement>(base_type);
  if (base_type == TYPE_object) {
    fn->add_body<GetListElementDependencies>();
  }
  return fn;
}

class CombineLists : public TupleCallBody {
 private:
  Type *m_base_type;

 public:
  CombineLists(Type *base_type) : m_base_type(std::move(base_type))
  {
  }

  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list1 = fn_in.relocate_out<SharedList>(0);
    auto list2 = fn_in.relocate_out<SharedList>(1);

    list1 = list1->get_mutable();
    list1->extend__dynamic_copy(list2);

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

static SharedFunction build_combine_lists_function(Type *base_type, Type *list_type)
{
  FunctionBuilder builder;
  builder.add_input("List 1", list_type);
  builder.add_input("List 2", list_type);
  builder.add_output("List", list_type);

  std::string name = "Combine " + base_type->name() + " Lists";
  auto fn = builder.build(name);
  fn->add_body<CombineLists>(base_type);
  if (base_type == TYPE_object) {
    fn->add_body<CombineListsDependencies>();
  }
  return fn;
}

class ListLength : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    auto list = fn_in.relocate_out<SharedList>(0);
    uint length = list->size();
    fn_out.set<uint>(0, length);
  }
};

static SharedFunction build_list_length_function(Type *base_type, Type *list_type)
{
  FunctionBuilder builder;
  builder.add_input("List", list_type);
  builder.add_output("Length", TYPE_int32);

  std::string name = base_type->name() + " List Length";
  auto fn = builder.build(name);
  fn->add_body<ListLength>();
  return fn;
}

/* Build List Functions
 *************************************/

using FunctionPerType = Map<Type *, SharedFunction>;

struct ListFunctions {
  FunctionPerType m_create_empty;
  FunctionPerType m_from_element;
  FunctionPerType m_append;
  FunctionPerType m_get_element;
  FunctionPerType m_combine;
  FunctionPerType m_length;
};

static void insert_list_functions_for_type(ListFunctions &functions,
                                           Type *base_type,
                                           Type *list_type)
{
  functions.m_create_empty.add(base_type, build_create_empty_list_function(base_type, list_type));
  functions.m_from_element.add(base_type,
                               build_create_single_element_list_function(base_type, list_type));
  functions.m_append.add(base_type, build_append_function(base_type, list_type));
  functions.m_get_element.add(base_type, build_get_element_function(base_type, list_type));
  functions.m_combine.add(base_type, build_combine_lists_function(base_type, list_type));
  functions.m_length.add(base_type, build_list_length_function(base_type, list_type));
}

BLI_LAZY_INIT_STATIC(ListFunctions, get_list_functions)
{
  ListFunctions functions;
  insert_list_functions_for_type(functions, TYPE_float, TYPE_float_list);
  insert_list_functions_for_type(functions, TYPE_float3, TYPE_float3_list);
  insert_list_functions_for_type(functions, TYPE_int32, TYPE_int32_list);
  insert_list_functions_for_type(functions, TYPE_bool, TYPE_bool_list);
  insert_list_functions_for_type(functions, TYPE_object, TYPE_object_list);
  insert_list_functions_for_type(functions, TYPE_rgba_f, TYPE_rgba_f_list);
  insert_list_functions_for_type(functions, TYPE_string, TYPE_string_list);
  insert_list_functions_for_type(functions, TYPE_falloff, TYPE_falloff_list);
  return functions;
}

/* Access List Functions
 *************************************/

SharedFunction &GET_FN_empty_list(Type *base_type)
{
  FunctionPerType &functions = get_list_functions().m_create_empty;
  BLI_assert(functions.contains(base_type));
  return functions.lookup(base_type);
}

SharedFunction &GET_FN_list_from_element(Type *base_type)
{
  FunctionPerType &functions = get_list_functions().m_from_element;
  BLI_assert(functions.contains(base_type));
  return functions.lookup(base_type);
}

SharedFunction &GET_FN_append_to_list(Type *base_type)
{
  FunctionPerType &functions = get_list_functions().m_append;
  BLI_assert(functions.contains(base_type));
  return functions.lookup(base_type);
}

SharedFunction &GET_FN_get_list_element(Type *base_type)
{
  FunctionPerType &functions = get_list_functions().m_get_element;
  BLI_assert(functions.contains(base_type));
  return functions.lookup(base_type);
}

SharedFunction &GET_FN_combine_lists(Type *base_type)
{
  FunctionPerType &functions = get_list_functions().m_combine;
  BLI_assert(functions.contains(base_type));
  return functions.lookup(base_type);
}

SharedFunction &GET_FN_list_length(Type *base_type)
{
  FunctionPerType &functions = get_list_functions().m_length;
  BLI_assert(functions.contains(base_type));
  return functions.lookup(base_type);
}

Type *get_list_type(Type *base_type)
{
  SharedFunction &fn = GET_FN_append_to_list(base_type);
  return fn->input_type(0);
}

}  // namespace Functions
}  // namespace FN
