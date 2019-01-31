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

	class Composition {
	public:
		template<typename T>
		void add(const T *value)
		{
			this->m_elements.add(this->get_key<T>(), (void *)value);
		}

		template<typename T>
		inline const T *get() const
		{
			uint64_t key = this->get_key<T>();
			if (this->m_elements.contains(key)) {
				return (T *)this->m_elements.lookup(key);
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

		BLI::SmallMap<uint64_t, void *> m_elements;
	};

	class Type {
	public:
		const std::string &name() const
		{
			return this->m_name;
		}

		template<typename T>
		inline const T *extension() const
		{
			return this->m_extensions.get<T>();
		}

		template<typename T>
		void extend(const T *extension)
		{
			BLI_assert(this->m_extensions.get<T>() == nullptr);
			this->m_extensions.add(extension);
		}

	protected:
		std::string m_name;

	private:
		Composition m_extensions;
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

		template<typename T>
		inline const T *body() const
		{
			return this->m_bodies.get<T>();
		}

		template<typename T>
		void add_body(const T *body)
		{
			BLI_assert(this->m_bodies.get<T>() == nullptr);
			this->m_bodies.add(body);
		}

	private:
		const Signature m_signature;
		Composition m_bodies;
	};

} /* namespace FN */