#pragma once

#include "FN_core.hpp"

namespace FN {

	class ExecutionStack {
	private:
		SmallStack<const char *> m_stack;

	public:
		ExecutionStack() = default;

		void push(const char *info)
		{
			m_stack.push(info);
		}

		void pop()
		{
			m_stack.pop();
		}

		void print_traceback() const;
	};

	class ExecutionContext {
	private:
		ExecutionStack &m_stack;

	public:
		ExecutionContext(ExecutionStack &stack)
			: m_stack(stack) {}

		ExecutionStack &stack() const
		{
			return m_stack;
		}
	};

} /* namespace FN */