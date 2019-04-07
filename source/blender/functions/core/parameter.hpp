#pragma once

#include "type.hpp"

namespace FN {

	class Parameter {
	public:
		Parameter(const std::string &name, const SharedType &type)
			: m_name(name), m_type(type) {}

		const std::string &name() const
		{
			return m_name;
		}

		SharedType type() const
		{
			return m_type;
		}

		SharedType &type()
		{
			return m_type;
		}

		void print() const;

	private:
		const std::string m_name;
		SharedType m_type;
	};

	class InputParameter final : public Parameter {
	public:
		InputParameter(const std::string &name, const SharedType &type)
			: Parameter(name, type) {}
	};

	class OutputParameter final : public Parameter {
	public:
		OutputParameter(const std::string &name, const SharedType &type)
			: Parameter(name, type) {}
	};

	using InputParameters = SmallVector<InputParameter>;
	using OutputParameters = SmallVector<OutputParameter>;

} /* namespace FN */
