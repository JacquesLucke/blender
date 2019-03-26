#include "FN_tuple_call.hpp"

namespace FN {

	void ExecutionStack::print_traceback() const
	{
		std::cout << "Traceback:" << std::endl;
		for (StackFrame *frame : m_stack) {
			std::cout << " > " << frame->to_string() << std::endl;
		}
	}

} /* namespace FN */