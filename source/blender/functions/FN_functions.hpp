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

		inline void set(uint index, void *src)
		{
			BLI_assert(index < this->types.size());
			uint size = this->offsets[index + 1] - this->offsets[index];
			this->storage.copy_in(this->offsets[index], src, size);
		}

		inline void get(uint index, void *dst) const
		{
			BLI_assert(index < this->offsets.size());
			uint size = this->offsets[index + 1] - this->offsets[index];
			this->storage.copy_out(dst, this->offsets[index], size);
		}

		template<uint size>
		inline void set_static(uint index, void *src)
		{
			BLI_assert(index < this->types.size());
			this->storage.copy_in<size>(this->offsets[index], src);
		}

		template<uint size>
		inline void get_static(uint index, void *dst) const
		{
			BLI_assert(index < this->offsets.size());
			this->storage.copy_out(dst, this->offsets[index], size);
		}

	private:
		const SmallTypeVector types;
		SmallVector<int> offsets;
		SmallBuffer<> storage;
	};

	class Inputs : public ValueArray {
	public:
		Inputs(const Function &fn);

	private:
		const Function &fn;
	};

	class Outputs : public ValueArray {
	public:
		Outputs(const Function &fn);

	private:
		const Function &fn;
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
