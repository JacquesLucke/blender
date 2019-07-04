#pragma once

/**
 * When a function is executed using the tuple-call backend, there is always an execution context.
 * The context contains information about the current call stack. This information is important to
 * produce good error messages. The call stack of tuple-call functions can be very different from
 * the actual C/C++ call stack. Therefore it is handled separately.
 *
 * Obviously, setting up the call stack should be very efficient, since might be done very often
 * when e.g. a function is called for every vertex of a mesh. Nevertheless, it should contain a lot
 * of information in the case of an error.
 */

#include "FN_core.hpp"
#include "BLI_small_stack.hpp"

namespace FN {

using BLI::SmallStack;

class StackFrame {
 public:
  virtual std::string to_string() const = 0;

  virtual void handle_warning(StringRef UNUSED(msg)) const
  {
  }
};

class SourceInfoStackFrame : public StackFrame {
 private:
  SourceInfo *m_source;

 public:
  SourceInfoStackFrame(SourceInfo *source) : m_source(source)
  {
  }

  SourceInfo *source() const
  {
    return m_source;
  }

  std::string to_string() const override;
  void handle_warning(StringRef msg) const override;
};

class TextStackFrame : public StackFrame {
 private:
  const char *m_text;

 public:
  TextStackFrame(const char *text) : m_text(text)
  {
  }

  const char *text() const
  {
    return m_text;
  }

  std::string to_string() const override;
};

class ExecutionStack {
 private:
  SmallStack<StackFrame *, 10> m_stack;

 public:
  ExecutionStack() = default;

  void push(StackFrame *frame)
  {
    m_stack.push(frame);
  }

  void pop()
  {
    m_stack.pop();
  }

  void print_traceback() const;

  StackFrame **begin()
  {
    return m_stack.begin();
  }

  StackFrame **end()
  {
    return m_stack.end();
  }
};

class ExecutionContext {
 private:
  ExecutionStack &m_stack;

 public:
  ExecutionContext(ExecutionStack &stack) : m_stack(stack)
  {
  }

  ExecutionStack &stack() const
  {
    return m_stack;
  }

  void print_with_traceback(StringRef msg);
  void log_warning(StringRef msg);
};

} /* namespace FN */
