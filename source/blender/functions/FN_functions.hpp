#pragma once

#include <string>
#include <type_traits>

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"

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

	class Tuple {
	public:
		Tuple() {}
		Tuple(const SmallTypeVector &types)
			: types(types)
		{
			int total_size = 0;
			for (const Type *type : types) {
				this->offsets.append(total_size);
				this->initialized.append(false);
				total_size += type->size();
			}
			this->offsets.append(total_size);
			this->data = std::malloc(total_size);
		}

		~Tuple()
		{
			std::free(this->data);
		}

		template<typename T>
		inline void set(uint index, const T &value)
		{
			BLI_assert(index < this->types.size());
			BLI_assert(sizeof(T) == this->element_size(index));

			if (std::is_trivial<T>::value) {
				std::memcpy((char *)this->data + this->offsets[index], &value, sizeof(T));
			}
			else {
				const T *begin = &value;
				const T *end = begin + 1;
				T *dst = (T *)((char *)this->data + this->offsets[index]);

				if (this->initialized[index]) {
					std::copy(begin, end, dst);
				}
				else {
					std::uninitialized_copy(begin, end, dst);
					this->initialized[index] = true;
				}
			}
		}

		template<typename T>
		inline const T &get(uint index) const
		{
			BLI_assert(index < this->types.size());
			BLI_assert(sizeof(T) == this->element_size(index));

			if (!std::is_trivial<T>::value) {
				BLI_assert(this->initialized[index]);
			}

			return *(T *)((char *)this->data + this->offsets[index]);;
		}

	private:
		inline uint element_size(uint index) const
		{
			return this->offsets[index + 1] - this->offsets[index];
		}

		const SmallTypeVector types;
		SmallVector<uint> offsets;
		SmallVector<bool> initialized;
		void *data;
	};

	class Inputs : public Tuple {
	public:
		Inputs(const Function &fn);

	private:
		const Function &fn;
	};

	class Outputs : public Tuple {
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
