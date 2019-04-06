#include <cmath>

#include "scalar_math.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_llvm.hpp"

#include "BLI_lazy_init.hpp"

namespace FN { namespace Functions {

	using namespace Types;

	static SharedFunction get_math_function__one_input(std::string name)
	{
		auto fn = SharedFunction::New(name, Signature({
			InputParameter("Value", get_float_type()),
		}, {
			OutputParameter("Result", get_float_type()),
		}));
		return fn;
	}

	static SharedFunction get_math_function__two_inputs(std::string name)
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
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a + b);
		}
	};

	class GenAddFloats : public LLVMBuildIRBody {
		void build_ir(
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &UNUSED(settings)) const override
		{
			auto output = builder.CreateFAdd(
				interface.get_input(0),
				interface.get_input(1));
			interface.set_output(0, output);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_add_floats)
	{
		auto fn = get_math_function__two_inputs("Add Floats");
		//fn->add_body(new AddFloats());
		fn->add_body(new GenAddFloats());
		return fn;
	}


	class MultiplyFloats : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a * b);
		}
	};

	class MultiplyFloatsGen : public LLVMBuildIRBody {
		void build_ir(
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &UNUSED(settings)) const override
		{
			auto output = builder.CreateFMul(
				interface.get_input(0),
				interface.get_input(1));
			interface.set_output(0, output);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_multiply_floats)
	{
		auto fn = get_math_function__two_inputs("Multiply Floats");
		fn->add_body(new MultiplyFloats());
		fn->add_body(new MultiplyFloatsGen());
		return fn;
	}


	class MinFloats : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? a : b);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_min_floats)
	{
		auto fn = get_math_function__two_inputs("Minimum");
		fn->add_body(new MinFloats());
		return fn;
	}


	class MaxFloats : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, (a < b) ? b : a);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_max_floats)
	{
		auto fn = get_math_function__two_inputs("Maximum");
		fn->add_body(new MaxFloats());
		return fn;
	}


	class MapRange : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
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

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_map_range)
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


	class SinFloat : public TupleCallBody {
		void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			float a = fn_in.get<float>(0);
			fn_out.set<float>(0, std::sin(a));
		}
	};

	class SinFloatGen : public LLVMBuildIRBody {
		void build_ir(
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &UNUSED(settings)) const override
		{
			auto output = builder.CreateSin(interface.get_input(0));
			interface.set_output(0, output);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, GET_FN_sin_float)
	{
		auto fn = get_math_function__one_input("Sin");
		fn->add_body(new SinFloat());
		fn->add_body(new SinFloatGen());
		return fn;
	}

} } /* namespace FN::Functions */