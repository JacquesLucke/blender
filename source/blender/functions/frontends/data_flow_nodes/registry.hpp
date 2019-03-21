#pragma once

#include "inserters.hpp"

namespace FN { namespace DataFlowNodes {

	void initialize_socket_inserters(GraphInserters &inserters);
	void register_node_inserters(GraphInserters &inserters);
	void register_conversion_inserters(GraphInserters &inserters);

} } /* namespace FN::DataFlowNodes */