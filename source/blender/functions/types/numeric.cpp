#include "numeric.hpp"
#include "BLI_lazy_init.hpp"

namespace FN { namespace Types {

	LAZY_INIT_REF__NO_ARG(SharedType, get_float_type)
	{
		SharedType type = SharedType::New("Float");
		type->extend(new CPPTypeInfoForType<float>());
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, get_int32_type)
	{
		SharedType type = SharedType::New("Int32");
		type->extend(new CPPTypeInfoForType<int32_t>());
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, get_fvec3_type)
	{
		SharedType type = SharedType::New("FloatVector3D");
		type->extend(new CPPTypeInfoForType<Vector>());
		return type;
	}

	LAZY_INIT_REF__NO_ARG(SharedType, get_float_list_type)
	{
		SharedType type = SharedType::New("Float List");
		type->extend(new CPPTypeInfoForType<SmallVector<float>>());
		return type;
	}

} } /* namespace FN::Types */