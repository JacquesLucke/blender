#include "data_flow_graph.hpp"

namespace FN {
	const Type *Socket::type() const
	{
		if (this->m_is_output) {
			return this->node()->signature().outputs()[this->m_index];
		}
		else {
			return this->node()->signature().inputs()[this->m_index];
		}
	}
};