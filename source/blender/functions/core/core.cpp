#include "core.hpp"

namespace FN {

	SmallTypeVector Signature::input_types() const
	{
		SmallTypeVector types;
		for (const InputParameter &param : this->inputs()) {
			types.append(param.type());
		}
		return types;
	}

	SmallTypeVector Signature::output_types() const
	{
		SmallTypeVector types;
		for (const OutputParameter &param : this->outputs()) {
			types.append(param.type());
		}
		return types;
	}

	bool Signature::has_interface(
		const SmallTypeVector &inputs,
		const SmallTypeVector &outputs) const
	{
		return (true
			&& SmallTypeVector::all_equal(this->input_types(), inputs)
			&& SmallTypeVector::all_equal(this->output_types(), outputs));
	}


	/* Printing
	 ***************************************/

	void Parameter::print() const
	{
		std::cout << this->type()->name() << " - " << this->name();
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

	void Function::print() const
	{
		std::cout << "Function: " << this->name() << std::endl;
		this->signature().print("  ");
	}

} /* namespace FN */