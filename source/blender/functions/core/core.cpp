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

} /* namespace FN */