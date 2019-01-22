#pragma once

#include "../FN_functions.hpp"

namespace FN::Types {

	class FloatType : public FN::Type {
	public:
		FloatType()
		{
			this->m_name = "Float";
			this->m_size = sizeof(float);
		}
	};

	class Int32Type : public FN::Type {
	public:
		Int32Type()
		{
			this->m_name = "Int32";
			this->m_size = sizeof(int32_t);
		}
	};

	template<uint N>
	class FloatVectorType : public FN::Type {
	public:
		FloatVectorType()
		{
			this->m_name = "FloatVector" + std::to_string(N) + "D";
			this->m_size = sizeof(float) * N;
		}
	};

	static const FloatType *float_ty = new FloatType();
	static const Int32Type *int32_ty = new Int32Type();
	static const FloatVectorType<3> *floatvec3d_ty = new FloatVectorType<3>();

} /* namespace FN::Types */