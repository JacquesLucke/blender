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

Inputs *Inputs::New(Function *fn)
{
	return nullptr;
}

bool Inputs::set(uint index, void *value)
{
	return false;
}

bool Function::call(Inputs *fn_in, Outputs *fn_out)
{
	return false;
}