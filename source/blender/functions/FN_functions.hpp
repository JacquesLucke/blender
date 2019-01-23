#pragma once

#include <string>

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"
#include "BLI_small_buffer.hpp"

namespace FN {
	using namespace BLI;

	class Type;
	class Inputs;
	class Outputs;
	class Signature;
	class Function;

	using SmallTypeVector = SmallVector<const Type *>;

	class Type {
	public:
		const std::string &name() const;
		const uint size() const;

	protected:
		std::string m_name;
		uint m_size;
	};

	class ValueArray {
	public:
		ValueArray() {};
		ValueArray(const SmallTypeVector &types);
		void set(uint index, void *src);
		void get(uint index, void *dst) const;

	private:
		const SmallTypeVector types;
		SmallVector<int> offsets;
		SmallBuffer<> storage;
	};

	class Inputs {
	public:
		Inputs(const Function &fn);

		inline void set(uint index, void *src)
		{ this->values.set(index, src); }
		inline void get(uint index, void *dst) const
		{ this->values.get(index, dst); }

	private:
		const Function &fn;
		ValueArray values;
	};

	class Outputs {
	public:
		Outputs(const Function &fn);

		inline void set(uint index, void *src)
		{ this->values.set(index, src); }
		inline void get(uint index, void *dst) const
		{ this->values.get(index, dst); }

	private:
		const Function &fn;
		ValueArray values;
	};

	class Signature {
	public:
		Signature()
			: m_inputs({}), m_outputs({}) {}

		Signature(const SmallTypeVector &inputs, const SmallTypeVector &outputs)
			: m_inputs(inputs), m_outputs(outputs) {}

		~Signature() {}

		inline const SmallTypeVector &inputs() const
		{ return this->m_inputs; }
		inline const SmallTypeVector &outputs() const
		{ return this->m_outputs; }

	private:
		const SmallTypeVector m_inputs;
		const SmallTypeVector m_outputs;
	};

	class Function {
	public:
		Function(const Signature &signature)
			: m_signature(signature) {}

		virtual ~Function();

		virtual bool call(const Inputs &fn_in, Outputs &fn_out) = 0;

		const Signature &signature() const;

	private:
		const Signature m_signature;
	};

} /* namespace FN */
