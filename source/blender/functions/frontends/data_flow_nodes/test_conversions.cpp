#include "registry.hpp"

#include "FN_functions.hpp"

namespace FN { namespace DataFlowNodes {

	void register_conversion_inserters(GraphInserters &inserters)
	{
		inserters.reg_conversion_function("Integer", "Float", Functions::int32_to_float);
		inserters.reg_conversion_function("Float", "Integer", Functions::float_to_int32);
	}

} } /* namespace FN::DataFlowNodes */