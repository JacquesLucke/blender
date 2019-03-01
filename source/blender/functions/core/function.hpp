#pragma once

#include "signature.hpp"

namespace FN {

	class Function final {
	public:
		Function(const std::string &name, const Signature &signature)
			: m_name(name), m_signature(signature) {}

		Function(const Signature &signature)
			: Function("Function", signature) {}

		~Function() = default;

		const std::string &name() const
		{
			return m_name;
		}

		inline const Signature &signature() const
		{
			return m_signature;
		}

		template<typename T>
		inline const T *body() const
		{
			return m_bodies.get<T>();
		}

		template<typename T>
		void add_body(const T *body)
		{
			BLI_assert(m_bodies.get<T>() == nullptr);
			m_bodies.add(body);
		}

		void print() const;

	private:
		const std::string m_name;
		const Signature m_signature;
		Composition m_bodies;
	};

	using SharedFunction = Shared<Function>;

} /* namespace FN */