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

		template<typename T>
		SmallVector<T *> input_extensions() const
		{
			SmallVector<T *> extensions;
			for (InputParameter &param : m_inputs) {
				T *ext = param.type()->extension<T>();
				BLI_assert(ext);
				extensions.append(ext);
			}
			return extensions;
		}

		template<typename T>
		SmallVector<T *> output_extensions() const
		{
			SmallVector<T *> extensions;
			for (OutputParameter &param : m_outputs) {
				T *ext = param.type()->extension<T>();
				BLI_assert(ext);
				extensions.append(ext);
			}
			return extensions;
		}

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
