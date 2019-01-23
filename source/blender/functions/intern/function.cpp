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
	: fn(fn), values(fn.signature().inputs()) { }

Outputs::Outputs(const Function &fn)
	: fn(fn), values(fn.signature().outputs()) { }



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

void ValueArray::set(uint index, void *src)
{
	BLI_assert(index < this->types.size());
	uint size = this->offsets[index + 1] - this->offsets[index];
	this->storage.copy_in(this->offsets[index], src, size);
}

void ValueArray::get(uint index, void *dst) const
{
	BLI_assert(index < this->offsets.size());
	this->storage.copy_out(
		dst,
		this->offsets[index],
		this->types[index]->size());
}


Function::~Function()
{
}

const Signature &Function::signature() const
{
	return this->m_signature;
}