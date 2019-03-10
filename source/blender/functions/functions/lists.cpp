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

	LAZY_INIT_REF__NO_ARG(SharedFunction, append_float)
	{
		auto fn = SharedFunction::New("Append Float", Signature({
			InputParameter("List", get_float_list_type()),
			InputParameter("Value", get_float_type()),
		}, {
			OutputParameter("List", get_float_list_type()),
		}));
		fn->add_body(new AppendToList<float>());
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

	LAZY_INIT_REF__NO_ARG(SharedFunction, get_float_list_element)
	{
		auto fn = SharedFunction::New("Get Float List Element", Signature({
			InputParameter("List", get_float_list_type()),
			InputParameter("Index", get_int32_type()),
			InputParameter("Fallback", get_float_type()),
		}, {
			OutputParameter("Element", get_float_type()),
		}));
		fn->add_body(new GetListElement<float>());
		return fn;
	}


	template<typename T>
	class CombineList : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out) const override
		{
			auto list1 = fn_in.relocate_out<SharedList<T>>(0);
			auto list2 = fn_in.relocate_out<SharedList<T>>(1);

			list1 = list1->get_mutable();
			list1->extend(list2.ptr());

			fn_out.move_in(0, list1);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, combine_float_lists)
	{
		SharedType &float_list_ty = get_float_list_type();

		auto fn = SharedFunction::New("Combine Float List", Signature({
			InputParameter("List 1", float_list_ty),
			InputParameter("List 2", float_list_ty),
		}, {
			OutputParameter("List", float_list_ty),
		}));
		fn->add_body(new CombineList<float>());
		return fn;
	}

} } /* namespace FN::Functions */