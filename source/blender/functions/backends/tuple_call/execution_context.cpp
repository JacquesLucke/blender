#include "FN_tuple_call.hpp"

namespace FN {

	void ExecutionStack::print_traceback() const
	{
		std::cout << "Traceback:" << std::endl;
		for (StackFrame *frame : m_stack) {
			std::cout << " > " << frame->to_string() << std::endl;
		}
	}

	std::string SourceInfoStackFrame::to_string() const
	{
		if (m_source == nullptr) {
			return "<unknown source>";
		} else {
			return m_source->to_string();
		}
	}

	std::string TextStackFrame::to_string() const
	{
		return std::string(m_text);
	}

} /* namespace FN */