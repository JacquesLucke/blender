#include "FN_functions.hpp"

using namespace FN;

const std::string &Type::name() const
{
	return this->m_name;
}

const uint Type::size() const
{
	return this->m_size;
}


Inputs::Inputs(const Function &fn)
	: ValueArray(fn.signature().inputs()), fn(fn) { }

Outputs::Outputs(const Function &fn)
	: ValueArray(fn.signature().outputs()), fn(fn) { }



ValueArray::ValueArray(const SmallTypeVector &types)
	: types(types)
{
	int total_size = 0;
	for (const Type *type : types) {
		this->offsets.append(total_size);
		total_size += type->size();
	}
	this->offsets.append(total_size);
	this->storage = SmallBuffer<>(total_size);
}

Function::~Function()
{
}

const Signature &Function::signature() const
{
	return this->m_signature;
}