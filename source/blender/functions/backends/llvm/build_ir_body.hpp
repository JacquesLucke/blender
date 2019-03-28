#pragma once

#include "builder.hpp"

namespace FN {

	class BuildIRSettings {
	private:
		bool m_maintain_stack = true;

	public:
		bool maintain_stack() const
		{
			return m_maintain_stack;
		}
	};

	class CodeInterface {
	private:
		LLVMValues &m_inputs;
		LLVMValues &m_outputs;
		llvm::Value *m_context_ptr;

	public:
		CodeInterface(
			LLVMValues &inputs,
			LLVMValues &outputs,
			llvm::Value *context_ptr = nullptr)
			: m_inputs(inputs),
			  m_outputs(outputs),
			  m_context_ptr(context_ptr) {}

		llvm::Value *get_input(uint index)
		{
			return m_inputs[index];
		}

		void set_output(uint index, llvm::Value *value)
		{
			m_outputs[index] = value;
		}

		const LLVMValues &inputs()
		{
			return m_inputs;
		}

		llvm::Value *context_ptr() const
		{
			return m_context_ptr;
		}
	};

	class LLVMBuildIRBody : public FunctionBody {
	public:
		BLI_COMPOSITION_DECLARATION(LLVMBuildIRBody);

		virtual ~LLVMBuildIRBody() {};

		virtual void build_ir(
			CodeBuilder &builder,
			CodeInterface &interface,
			const BuildIRSettings &settings) const = 0;
	};

}