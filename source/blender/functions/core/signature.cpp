#include "signature.hpp"

namespace FN {

	TypeVector Signature::input_types() const
	{
		TypeVector types;
		for (const InputParameter &param : this->inputs()) {
			types.append(param.type());
		}
		return types;
	}

	TypeVector Signature::output_types() const
	{
		TypeVector types;
		for (const OutputParameter &param : this->outputs()) {
			types.append(param.type());
		}
		return types;
	}

	bool Signature::has_interface(
		const TypeVector &inputs,
		const TypeVector &outputs) const
	{
		return (true
			&& TypeVector::all_equal(this->input_types(), inputs)
			&& TypeVector::all_equal(this->output_types(), outputs));
	}

	bool Signature::has_interface(
		const Signature &other) const
	{
		return this->has_interface(other.input_types(), other.output_types());
	}

	void Signature::print(std::string indent) const
	{
		std::cout << indent << "Inputs:" << std::endl;
		for (InputParameter &param : this->inputs()) {
			std::cout << indent << "  ";
			param.print();
			std::cout << std::endl;
		}
		std::cout << indent << "Outputs:" << std::endl;
		for (OutputParameter &param : this->outputs()) {
			std::cout << indent << "  ";
			param.print();
			std::cout << std::endl;
		}
	}

} /* namespace FN */
