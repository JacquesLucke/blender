#pragma once

#include <string>

#include "BLI_utildefines.h"
#include "BLI_small_vector.hpp"

namespace FN {
	using namespace BLI;

	class Type;
	class Inputs;
	class Outputs;
	class Signature;
	class Function;

	class Type {
	public:
		const std::string &name() const;
		const uint size() const;

	protected:
		std::string m_name;
		uint m_size;
	};

	class Inputs {
	public:
		static Inputs *New(Function *fn);

		bool set(uint index, void *value);

	private:
		Inputs() {}

		Function *fn;
	};

	class Outputs {
	public:
		static Outputs *New(Function *fn);

		bool get(uint index, void *value);

	private:
		Outputs() {}

		Function *fn;
	};

	class Signature {
	private:
		SmallVector<Type *> inputs;
		SmallVector<Type *> outputs;
	};

	class Function {
	public:
		bool call(Inputs *fn_in, Outputs *fn_out);

	private:
		Signature *signature;
	};
} /* namespace FN */
