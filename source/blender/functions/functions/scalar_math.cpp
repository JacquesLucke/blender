#include "scalar_math.hpp"
#include "FN_functions_types.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	static SharedFunction get_simple_math_function(std::string name)
	{
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("A", get_float_type()),
			InputParameter("B", get_float_type()),
		}, {
			OutputParameter("Result", get_float_type()),
		}));
		return fn;
	}


	class AddFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a + b);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, add_floats)
	{
		auto fn = get_simple_math_function("Add Floats");
		fn->add_body(new AddFloats());
		return fn;
	}


	class MultiplyFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a * b);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, multiply_floats)
	{
		auto fn = get_simple_math_function("Multiply Floats");
		fn->add_body(new MultiplyFloats());
		return fn;
	}


	class MinFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? a : b);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, min_floats)
	{
		auto fn = get_simple_math_function("Minimum");
		fn->add_body(new MinFloats());
		return fn;
	}


	class MaxFloats : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? b : a);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, max_floats)
	{
		auto fn = get_simple_math_function("Maximum");
		fn->add_body(new MaxFloats());
		return fn;
	}


	class MapRange : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			float value = fn_in.get<float>(0);
			float from_min = fn_in.get<float>(1);
			float from_max = fn_in.get<float>(2);
			float to_min = fn_in.get<float>(3);
			float to_max = fn_in.get<float>(4);

			float from_range = from_max - from_min;
			float to_range = to_max - to_min;

			float result;
			if (from_range == 0) {
				result = to_min;
			}
			else {
				float t = (value - from_min) / from_range;
				CLAMP(t, 0.0f, 1.0f);
				result = t * to_range + to_min;
			}

			fn_out.set<float>(0, result);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, map_range)
	{
		auto fn = SharedFunction::New("Map Range", Signature({
			InputParameter("Value", get_float_type()),
			InputParameter("From Min", get_float_type()),
			InputParameter("From Max", get_float_type()),
			InputParameter("To Min", get_float_type()),
			InputParameter("To Max", get_float_type()),
		}, {
			OutputParameter("Value", get_float_type()),
		}));
		fn->add_body(new MapRange());
		return fn;
	}

} } /* namespace FN::Functions */