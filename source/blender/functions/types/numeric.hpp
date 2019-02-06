#pragma once

#include "../FN_functions.hpp"

namespace FN::Types {

	class FloatType : public FN::Type {
	public:
		FloatType()
		{
			this->m_name = "Float";
			this->extend(new FN::TypeSize(sizeof(float)));
		}
	};

	class Int32Type : public FN::Type {
	public:
		Int32Type()
		{
			this->m_name = "Int32";
			this->extend(new FN::TypeSize(sizeof(int32_t)));
		}
	};

	template<uint N>
	class FloatVectorType : public FN::Type {
	public:
		FloatVectorType()
		{
			this->m_name = "FloatVector" + std::to_string(N) + "D";
			this->extend(new FN::TypeSize(sizeof(float) * N));
		}
	};

	static SharedType float_ty = SharedType::FromPointer(new FloatType());
	static SharedType int32_ty = SharedType::FromPointer(new Int32Type());
	static SharedType floatvec3d_ty = SharedType::FromPointer(new FloatVectorType<3>());

} /* namespace FN::Types */