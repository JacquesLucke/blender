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
	: Tuple(fn.signature().inputs()), fn(fn) { }

Outputs::Outputs(const Function &fn)
	: Tuple(fn.signature().outputs()), fn(fn) { }

Function::~Function()
{
}

const Signature &Function::signature() const
{
	return this->m_signature;
}