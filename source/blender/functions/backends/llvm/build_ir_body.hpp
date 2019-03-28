#pragma once

#include "builder.hpp"

namespace FN {

	class BuildIRSettings {

	};

	class CodeInterface {
	private:
		LLVMValues &m_inputs;
		LLVMValues &m_outputs;

	public:
		CodeInterface(LLVMValues &inputs, LLVMValues &outputs)
			: m_inputs(inputs), m_outputs(outputs) {}

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