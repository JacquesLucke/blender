#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	template<typename T>
	class CreateEmptyList : public TupleCallBody {
		void call(Tuple &UNUSED(fn_in), Tuple &fn_out) const override
		{
			auto list = SharedList<T>::New();
			fn_out.move_in(0, list);
		}
	};

	template<typename T>
	SharedFunction build_create_empty_list_function(
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Create Empty " + base_type->name() + " List";
		auto fn = SharedFunction::New(name, Signature({}, {
			OutputParameter("List", list_type),
		}));
		fn->add_body(new CreateEmptyList<T>());
		return fn;
	}


	template<typename T>
	class AppendToList : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out) const override
		{
			auto list = fn_in.relocate_out<SharedList<T>>(0);
			T value = fn_in.relocate_out<T>(1);

			list = list->get_mutable();
			list->append(value);

			fn_out.move_in(0, list);
		}
	};

	template<typename T>
	SharedFunction build_append_function(
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Append " + base_type->name();
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("List", list_type),
			InputParameter("Value", base_type),
		}, {
			OutputParameter("List", list_type),
		}));
		fn->add_body(new AppendToList<T>());
		return fn;
	}


	template<typename T>
	class GetListElement : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out) const override
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
	SharedFunction build_get_element_function(
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Get " + base_type->name() + " List Element";
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("List", list_type),
			InputParameter("Index", get_int32_type()),
			InputParameter("Fallback", base_type),
		}, {
			OutputParameter("Element", base_type),
		}));
		fn->add_body(new GetListElement<T>());
		return fn;
	}


	template<typename T>
	class CombineLists : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out) const override
		{
			auto list1 = fn_in.relocate_out<SharedList<T>>(0);
			auto list2 = fn_in.relocate_out<SharedList<T>>(1);

			list1 = list1->get_mutable();
			list1->extend(list2.ptr());

			fn_out.move_in(0, list1);
		}
	};

	template<typename T>
	SharedFunction build_combine_lists_function(
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Combine " + base_type->name() + " Lists";
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("List 1", list_type),
			InputParameter("List 2", list_type),
		}, {
			OutputParameter("List", list_type),
		}));
		fn->add_body(new CombineLists<T>());
		return fn;
	}


	/* Build List Functions
	 *************************************/

	using FunctionPerType = SmallMap<SharedType, SharedFunction>;

	struct ListFunctions {
		FunctionPerType m_create_empty;
		FunctionPerType m_append;
		FunctionPerType m_get_element;
		FunctionPerType m_combine;
	};

	template<typename T>
	void insert_list_functions_for_type(
		ListFunctions &functions,
		SharedType &base_type,
		SharedType &list_type)
	{
		functions.m_create_empty.add(
			base_type,
			build_create_empty_list_function<T>(base_type, list_type));
		functions.m_append.add(
			base_type,
			build_append_function<T>(base_type, list_type));
		functions.m_get_element.add(
			base_type,
			build_get_element_function<T>(base_type, list_type));
		functions.m_combine.add(
			base_type,
			build_combine_lists_function<T>(base_type, list_type));
	}

	LAZY_INIT_REF_STATIC__NO_ARG(ListFunctions, get_list_functions)
	{
		ListFunctions functions;
		insert_list_functions_for_type<float>(
			functions, get_float_type(), get_float_list_type());
		insert_list_functions_for_type<Vector>(
			functions, get_fvec3_type(), get_fvec3_list_type());
		insert_list_functions_for_type<int32_t>(
			functions, get_int32_type(), get_int32_list_type());
		return functions;
	}


	/* Access List Functions
	 *************************************/

	SharedFunction &empty_list(SharedType &base_type)
	{
		FunctionPerType &functions = get_list_functions().m_create_empty;
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
	}

	SharedFunction &append_to_list(SharedType &base_type)
	{
		FunctionPerType &functions = get_list_functions().m_append;
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
	}

	SharedFunction &get_list_element(SharedType &base_type)
	{
		FunctionPerType &functions = get_list_functions().m_get_element;
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
	}

	SharedFunction &combine_lists(SharedType &base_type)
	{
		FunctionPerType &functions = get_list_functions().m_combine;
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
	}

} } /* namespace FN::Functions */