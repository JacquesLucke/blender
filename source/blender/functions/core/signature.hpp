#pragma once

#include "parameter.hpp"

namespace FN {

	class Signature {
	public:
		Signature() = default;
		~Signature() = default;

		Signature(const InputParameters &inputs, const OutputParameters &outputs)
			: m_inputs(inputs), m_outputs(outputs) {}

		inline const InputParameters &inputs() const
		{
			return m_inputs;
		}

		inline const OutputParameters &outputs() const
		{
			return m_outputs;
		}

		SmallTypeVector input_types() const;
		SmallTypeVector output_types() const;

		bool has_interface(
			const SmallTypeVector &inputs,
			const SmallTypeVector &outputs) const;

		bool has_interface(
			const Signature &other) const;

		void print(std::string indent = "") const;

	private:
		const InputParameters m_inputs;
		const OutputParameters m_outputs;
	};

} /* namespace FN */