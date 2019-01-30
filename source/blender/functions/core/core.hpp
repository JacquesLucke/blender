#pragma once

#include <string>

#include "BLI_small_vector.hpp"
#include "BLI_small_map.hpp"

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

	class FunctionBodies {
	private:
		BLI::SmallMap<uint64_t, void *> m_bodies;

	public:
		template<typename T>
		void add(const T *body)
		{
			this->m_bodies.add(this->get_key<T>(), (void *)body);
		}

		template<typename T>
		inline const T *get() const
		{
			uint64_t key = this->get_key<T>();
			if (this->m_bodies.contains(key)) {
				return (T *)this->m_bodies.lookup(key);
			}
			else {
				return nullptr;
			}
		}

	private:
		template<typename T>
		static uint64_t get_key()
		{
			return (uint64_t)T::identifier;
		}
	};

	class Function {
	public:
		Function(const Signature &signature, const FunctionBodies &bodies)
			: m_signature(signature), m_bodies(bodies) {}

		virtual ~Function() {}

		inline const Signature &signature() const
		{
			return this->m_signature;
		}

		template<typename T>
		inline const T *body() const
		{
			return this->m_bodies.get<T>();
		}

	private:
		const Signature m_signature;
		const FunctionBodies m_bodies;
	};

} /* namespace FN */