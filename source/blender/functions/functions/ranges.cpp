#include "FN_core.hpp"
#include "FN_functions.hpp"
#include "FN_types.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	class FloatRange : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			int amount = fn_in.get<int>(0);
			float start = fn_in.get<float>(1);
			float step = fn_in.get<float>(2);

			if (amount < 0) {
				amount = 0;
			}

			auto list = SharedFloatList::New();
			float value = start;
			for (uint i = 0; i < amount; i++) {
				list->append(value);
				value += step;
			}

			fn_out.move_in(0, list);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_float_range)
	{
		auto fn = SharedFunction::New("Float Range", Signature({
			InputParameter("Amount", GET_TYPE_int32()),
			InputParameter("Start", GET_TYPE_float()),
			InputParameter("Step", GET_TYPE_float()),
		}, {
			OutputParameter("List", GET_TYPE_float_list()),
		}));
		fn->add_body(new FloatRange());
		return fn;
	}

} } /* namespace FN::Functions */