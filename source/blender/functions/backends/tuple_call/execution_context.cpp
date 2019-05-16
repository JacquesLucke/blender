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
  }
  else {
    return m_source->to_string();
  }
}

void SourceInfoStackFrame::handle_warning(StringRef msg) const
{
  if (m_source != nullptr) {
    m_source->handle_warning(msg);
  }
}

std::string TextStackFrame::to_string() const
{
  return std::string(m_text);
}

void ExecutionContext::print_with_traceback(StringRef msg)
{
  m_stack.print_traceback();
  std::cout << "-> " << msg << std::endl;
}

void ExecutionContext::log_warning(StringRef msg)
{
  for (StackFrame *frame : m_stack) {
    frame->handle_warning(msg);
  }
}

} /* namespace FN */
