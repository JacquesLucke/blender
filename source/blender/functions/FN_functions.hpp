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
		ValueArray(SmallTypeVector types);
		void set(uint index, void *src);
		void get(uint index, void *dst) const;

	private:
		SmallTypeVector types;
		SmallVector<int> offsets;
		SmallBuffer<> storage;
	};

	class Inputs {
	public:
		Inputs(Function &fn);

		inline void set(uint index, void *src)
		{ this->values.set(index, src); }
		inline void get(uint index, void *dst) const
		{ this->values.get(index, dst); }

	private:
		Function &fn;
		ValueArray values;
	};

	class Outputs {
	public:
		Outputs(Function &fn);

		inline void set(uint index, void *src)
		{ this->values.set(index, src); }
		inline void get(uint index, void *dst) const
		{ this->values.get(index, dst); }

	private:
		Function &fn;
		ValueArray values;
	};

	class Signature {
	public:
		Signature() {}
		Signature(SmallTypeVector inputs, SmallTypeVector outputs)
			: m_inputs(inputs), m_outputs(outputs) {}

		inline const SmallTypeVector &inputs() const
		{ return this->m_inputs; }
		inline const SmallTypeVector &outputs() const
		{ return this->m_outputs; }

	private:
		SmallTypeVector m_inputs;
		SmallTypeVector m_outputs;
	};

	class Function {
	public:
		bool call(Inputs &fn_in, Outputs &fn_out);

		inline const Signature &signature() const
		{ return this->m_signature; }

	private:

	protected:
		Signature m_signature;
	};
} /* namespace FN */
