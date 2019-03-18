#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

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

	LAZY_INIT_REF__NO_ARG(SharedFunction, append_float)
	{
		return build_append_function<float>(
			"Append Float",
			get_float_type(),
			get_float_list_type());
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, append_fvec3)
	{
		return build_append_function<Vector>(
			"Append Vector",
			get_fvec3_type(),
			get_fvec3_list_type());
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, append_int32)
	{
		return build_append_function<int32_t>(
			"Append Int32",
			get_int32_type(),
			get_int32_list_type());
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

	LAZY_INIT_REF__NO_ARG(SharedFunction, get_float_list_element)
	{
		return build_get_element_function<float>(
			"Get Float List Element",
			get_float_type(),
			get_float_list_type());
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, get_fvec3_list_element)
	{
		return build_get_element_function<Vector>(
			"Get Vector List Element",
			get_fvec3_type(),
			get_fvec3_list_type());
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, get_int32_list_element)
	{
		return build_get_element_function<int32_t>(
			"Get Int32 List Element",
			get_int32_type(),
			get_int32_list_type());
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
		fn->add_body(new CombineLists<float>());
		return fn;
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, combine_float_lists)
	{
		return build_combine_lists_function<float>(
			"Combine Float Lists",
			get_float_list_type());
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, combine_fvec3_lists)
	{
		return build_combine_lists_function<Vector>(
			"Combine Vector Lists",
			get_fvec3_list_type());
	}

	LAZY_INIT_REF__NO_ARG(SharedFunction, combine_int32_lists)
	{
		return build_combine_lists_function<int32_t>(
			"Combine Int32 Lists",
			get_int32_list_type());
	}

} } /* namespace FN::Functions */