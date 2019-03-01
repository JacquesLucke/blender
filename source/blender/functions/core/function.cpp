#include "function.hpp"

namespace FN {

	void Function::print() const
	{
		std::cout << "Function: " << this->name() << std::endl;
		this->signature().print("  ");
	}

} /* namespace FN */