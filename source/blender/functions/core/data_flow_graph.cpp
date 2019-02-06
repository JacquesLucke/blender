#include "data_flow_graph.hpp"

namespace FN {
	const SharedType &Socket::type() const
	{
		if (this->m_is_output) {
			return this->node()->signature().outputs()[this->m_index].type();
		}
		else {
			return this->node()->signature().inputs()[this->m_index].type();
		}
	}

	std::string Socket::name() const
	{
		if (this->m_is_output) {
			return this->node()->signature().outputs()[this->m_index].name();
		}
		else {
			return this->node()->signature().inputs()[this->m_index].name();
		}
	}
};