#pragma once

#include "FN_core.hpp"

namespace FN {

class StackFrame {
 public:
  virtual std::string to_string() const = 0;

  virtual void handle_warning(std::string UNUSED(msg)) const
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
  void handle_warning(std::string msg) const override;
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

  void print_with_traceback(std::string msg);
  void log_warning(std::string msg);
};

} /* namespace FN */
