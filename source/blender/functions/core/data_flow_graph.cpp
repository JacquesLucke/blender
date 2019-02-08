#include "data_flow_graph.hpp"

namespace FN {
	const SharedType &Socket::type() const
	{
		if (m_is_output) {
			return this->node()->signature().outputs()[m_index].type();
		}
		else {
			return this->node()->signature().inputs()[m_index].type();
		}
	}

	std::string Socket::name() const
	{
		if (m_is_output) {
			return this->node()->signature().outputs()[m_index].name();
		}
		else {
			return this->node()->signature().inputs()[m_index].name();
		}
	}
};