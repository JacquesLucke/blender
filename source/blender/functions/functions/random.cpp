#include "random.hpp"
#include "FN_types.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	static uint32_t random_int(uint32_t x)
	{
		x = (x<<13) ^ x;
		return x * (x * x * 15731 + 789221) + 1376312589;
	}

	static float random_float(uint32_t x)
	{
		x = random_int(x);
		return (float)x / 4294967296.0f;
	}


	class RandomNumber : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			uint32_t seed = fn_in.get<int32_t>(0);
			float min = fn_in.get<float>(1);
			float max = fn_in.get<float>(2);
			float result = random_float(seed) * (max - min) + min;
			fn_out.set<float>(0, result);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, random_number)
	{
		auto fn = SharedFunction::New("Random Number", Signature({
			InputParameter("Seed", get_int32_type()),
			InputParameter("Min", get_float_type()),
			InputParameter("Max", get_float_type()),
		}, {
			OutputParameter("Value", get_float_type()),
		}));
		fn->add_body(new RandomNumber());
		return fn;
	}

} } /* namespace FN::Functions */