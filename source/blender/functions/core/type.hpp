#pragma once

#include <string>
#include "BLI_composition.hpp"
#include "BLI_shared.hpp"

namespace FN {

	using namespace BLI;

	class Type final {
	public:
		Type() = delete;
		Type(const std::string &name)
			: m_name(name) {}

		const std::string &name() const
		{
			return m_name;
		}

		template<typename T>
		inline T *extension() const
		{
			return m_extensions.get<T>();
		}

		template<typename T>
		void extend(T *extension)
		{
			BLI_assert(m_extensions.get<T>() == nullptr);
			m_extensions.add(extension);
		}

	private:
		std::string m_name;
		Composition m_extensions;
	};

	using SharedType = Shared<Type>;
	using SmallTypeVector = SmallVector<SharedType>;

} /* namespace FN */