#include "numeric.hpp"

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

#define DEFAULT_TYPE(name, initializer) \
	SharedType TYPE_##name = SharedType::FromPointer(initializer); \
	SharedType &get_##name##_type() { return TYPE_##name; }

	DEFAULT_TYPE(float, new FloatType());
	DEFAULT_TYPE(int32, new Int32Type());
	DEFAULT_TYPE(fvec3, new FloatVectorType<3>());

} /* namespace FN::Types */