#pragma once

#include <string>
#include <iostream>

#include "BLI_composition.hpp"
#include "BLI_small_vector.hpp"
#include "BLI_small_map.hpp"
#include "BLI_shared.hpp"

namespace FN {

	using namespace BLI;

	class Type;
	class Signature;
	class Function;

	using SharedType = Shared<Type>;
	using SharedFunction = Shared<Function>;
	using SmallTypeVector = SmallVector<SharedType>;

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

	class Parameter {
	public:
		Parameter(const std::string &name, const SharedType &type)
			: m_name(name), m_type(type) {}

		const std::string &name() const
		{
			return m_name;
		}

		const SharedType &type() const
		{
			return m_type;
		}

	private:
		const std::string m_name;
		const SharedType m_type;
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

	class Signature {
	public:
		Signature() = default;
		~Signature() = default;

		Signature(const InputParameters &inputs, const OutputParameters &outputs)
			: m_inputs(inputs), m_outputs(outputs) {}

		inline const InputParameters &inputs() const
		{
			return m_inputs;
		}

		inline const OutputParameters &outputs() const
		{
			return m_outputs;
		}

		SmallTypeVector input_types() const
		{
			SmallTypeVector types;
			for (const InputParameter &param : this->inputs()) {
				types.append(param.type());
			}
			return types;
		}

		SmallTypeVector output_types() const
		{
			SmallTypeVector types;
			for (const OutputParameter &param : this->outputs()) {
				types.append(param.type());
			}
			return types;
		}

	private:
		const InputParameters m_inputs;
		const OutputParameters m_outputs;
	};

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

	private:
		const std::string m_name;
		const Signature m_signature;
		Composition m_bodies;
	};

} /* namespace FN */