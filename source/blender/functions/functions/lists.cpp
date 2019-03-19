#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;
	using FunctionPerType = SmallMap<SharedType, SharedFunction>;

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
		std::string name,
		SharedType &base_type,
		SharedType &list_type)
	{
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
	void insert_append_to_list_function(
		FunctionPerType &functions,
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Append " + base_type->name();
		SharedFunction fn = build_append_function<T>(name, base_type, list_type);
		functions.add(base_type, fn);
	}

	LAZY_INIT_REF_STATIC__NO_ARG(FunctionPerType, get_append_to_list_functions)
	{
		FunctionPerType functions;
		insert_append_to_list_function<float>(
			functions, get_float_type(), get_float_list_type());
		insert_append_to_list_function<Vector>(
			functions, get_fvec3_type(), get_fvec3_list_type());
		insert_append_to_list_function<int32_t>(
			functions, get_int32_type(), get_int32_list_type());
		return functions;
	}

	SharedFunction &append_to_list(SharedType &base_type)
	{
		FunctionPerType &functions = get_append_to_list_functions();
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
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
		std::string name,
		SharedType &base_type,
		SharedType &list_type)
	{
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
	void insert_get_element_function(
		FunctionPerType &functions,
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Get " + base_type->name() + " List Element";
		SharedFunction fn = build_get_element_function<T>(name, base_type, list_type);
		functions.add(base_type, fn);
	}

	LAZY_INIT_REF_STATIC__NO_ARG(FunctionPerType, get_get_element_functions)
	{
		FunctionPerType functions;
		insert_get_element_function<float>(
			functions, get_float_type(), get_float_list_type());
		insert_get_element_function<Vector>(
			functions, get_fvec3_type(), get_fvec3_list_type());
		insert_get_element_function<int32_t>(
			functions, get_int32_type(), get_int32_list_type());
		return functions;
	}

	SharedFunction &get_list_element(SharedType &base_type)
	{
		FunctionPerType &functions = get_get_element_functions();
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
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
		std::string name,
		SharedType &list_type)
	{
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("List 1", list_type),
			InputParameter("List 2", list_type),
		}, {
			OutputParameter("List", list_type),
		}));
		fn->add_body(new CombineLists<T>());
		return fn;
	}

	template<typename T>
	void insert_combine_lists_function(
		FunctionPerType &functions,
		SharedType &base_type,
		SharedType &list_type)
	{
		std::string name = "Combine " + base_type->name() + " Lists";
		SharedFunction fn = build_combine_lists_function<T>(name, list_type);
		functions.add(base_type, fn);
	}

	LAZY_INIT_REF_STATIC__NO_ARG(FunctionPerType, get_combine_lists_function)
	{
		FunctionPerType functions;
		insert_combine_lists_function<float>(
			functions, get_float_type(), get_float_list_type());
		insert_combine_lists_function<Vector>(
			functions, get_fvec3_type(), get_fvec3_list_type());
		insert_combine_lists_function<int32_t>(
			functions, get_int32_type(), get_int32_list_type());
		return functions;
	}

	SharedFunction &combine_lists(SharedType &base_type)
	{
		FunctionPerType &functions = get_combine_lists_function();
		BLI_assert(functions.contains(base_type));
		return functions.lookup_ref(base_type);
	}

} } /* namespace FN::Functions */