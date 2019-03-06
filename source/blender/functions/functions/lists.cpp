#include "lists.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	class AppendFloat : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			auto *list = fn_in.get<FloatList *>(0);
			float value = fn_in.get<float>(1);

			list = list->get_mutable(false);

			list->append(value);
			fn_out.set<FloatList *>(0, list);
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
		fn->add_body(new AppendFloat());
		return fn;
	}

} } /* namespace FN::Functions */