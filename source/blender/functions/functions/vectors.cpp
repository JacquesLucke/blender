#include "vectors.hpp"
#include "FN_types.hpp"

#include "FN_tuple_call.hpp"
#include "BLI_lazy_init.hpp"
#include "BLI_math.h"

namespace FN { namespace Functions {

	using namespace Types;

	class CombineVector : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector v;
			v.x = fn_in.get<float>(0);
			v.y = fn_in.get<float>(1);
			v.z = fn_in.get<float>(2);
			fn_out.set<Vector>(0, v);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, combine_vector)
	{
		auto fn = SharedFunction::New("Combine Vector", Signature({
			InputParameter("X", get_float_type()),
			InputParameter("Y", get_float_type()),
			InputParameter("Z", get_float_type()),
		}, {
			OutputParameter("Vector", get_fvec3_type()),
		}));
		fn->add_body(new CombineVector());
		return fn;
	}


	class SeparateVector : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector v = fn_in.get<Vector>(0);
			fn_out.set<float>(0, v.x);
			fn_out.set<float>(1, v.y);
			fn_out.set<float>(2, v.z);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, separate_vector)
	{
		auto fn = SharedFunction::New("Separate Vector", Signature({
			InputParameter("Vector", get_fvec3_type()),
		}, {
			OutputParameter("X", get_float_type()),
			OutputParameter("Y", get_float_type()),
			OutputParameter("Z", get_float_type()),
		}));
		fn->add_body(new SeparateVector());
		return fn;
	}


	class VectorDistance : public TupleCallBody {
		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			Vector a = fn_in.get<Vector>(0);
			Vector b = fn_in.get<Vector>(1);
			float distance = len_v3v3((float *)&a, (float *)&b);
			fn_out.set<float>(0, distance);
		}
	};

	LAZY_INIT_REF__NO_ARG(SharedFunction, vector_distance)
	{
		auto fn = SharedFunction::New("Vector Distance", Signature({
			InputParameter("A", get_fvec3_type()),
			InputParameter("B", get_fvec3_type()),
		}, {
			OutputParameter("Distance", get_float_type()),
		}));
		fn->add_body(new VectorDistance());
		return fn;
	}

} } /* namespace FN::Functions */