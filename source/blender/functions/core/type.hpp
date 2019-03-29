#pragma once

#include <string>
#include "BLI_composition.hpp"
#include "BLI_shared.hpp"

namespace FN {

	using namespace BLI;

	class TypeExtension {

	};

	class Type final : public RefCountedBase {
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
			static_assert(std::is_base_of<TypeExtension, T>::value, "");
			m_extensions.add(extension);
		}

	private:
		std::string m_name;
		Composition m_extensions;
	};

	using SharedType = AutoRefCount<Type>;
	using SmallTypeVector = SmallVector<SharedType>;

} /* namespace FN */