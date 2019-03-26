#include "FN_tuple_call.hpp"

namespace FN {

	void ExecutionStack::print_traceback() const
	{
		std::cout << "Traceback:" << std::endl;
		for (const char *info : m_stack) {
			std::cout << " > " << info << std::endl;
		}
	}

} /* namespace FN */