#pragma once

#include <string>

#include "BLI_small_vector.hpp"

namespace FN {

	using namespace BLI;

	class Type;
	class Signature;
	class Function;

	using SmallTypeVector = SmallVector<const Type *>;

	class Type {
	public:
		const std::string &name() const
		{
			return this->m_name;
		}

	protected:
		std::string m_name;

	public:
		/* will be removed */
		uint m_size;
	};

	class Signature {
	public:
		Signature() = default;
		~Signature() = default;

		Signature(const SmallTypeVector &inputs, const SmallTypeVector &outputs)
			: m_inputs(inputs), m_outputs(outputs) {}

		inline const SmallTypeVector &inputs() const
		{
			return this->m_inputs;
		}

		inline const SmallTypeVector &outputs() const
		{
			return this->m_outputs;
		}

	private:
		const SmallTypeVector m_inputs;
		const SmallTypeVector m_outputs;
	};

	class Function {
	public:
		Function(const Signature &signature)
			: m_signature(signature) {}

		virtual ~Function() {}

		inline const Signature &signature() const
		{
			return this->m_signature;
		}

	private:
		const Signature m_signature;
	};

} /* namespace FN */