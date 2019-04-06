#pragma once

#include "../FN_core.hpp"

namespace FN { namespace Types {

	struct Vector {
		float x, y, z;

		Vector() = default;

		Vector(float x, float y, float z)
			: x(x), y(y), z(z) {}

		Vector(float *vec)
			: x(vec[0]), y(vec[1]), z(vec[2]) {}
	};

	SharedType &GET_TYPE_float();
	SharedType &GET_TYPE_int32();
	SharedType &GET_TYPE_fvec3();

} } /* namespace FN::Types */