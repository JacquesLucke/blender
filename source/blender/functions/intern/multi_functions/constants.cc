#include "constants.h"

namespace FN {

void MF_GenericConstantValue::value_to_string(std::stringstream &ss,
                                              const CPPType &type,
                                              const void *value)
{
  if (type == CPP_TYPE<float>()) {
    ss << (*(float *)value);
  }
  else if (type == CPP_TYPE<int>()) {
    ss << *(int *)value;
  }
  else if (type == CPP_TYPE<BLI::float3>()) {
    ss << *(BLI::float3 *)value;
  }
  else if (type == CPP_TYPE<bool>()) {
    ss << ((*(bool *)value) ? "true" : "false");
  }
  else if (type == CPP_TYPE<std::string>()) {
    ss << "\"" << *(std::string *)value << "\"";
  }
  else {
    ss << "Value";
  }
}

MF_GenericConstantValue::MF_GenericConstantValue(const CPPType &type, const void *value)
    : m_value(value)
{
  MFSignatureBuilder signature = this->get_builder("Constant " + type.name());
  std::stringstream ss;
  MF_GenericConstantValue::value_to_string(ss, type, value);
  signature.single_output(ss.str(), type);
}

void MF_GenericConstantValue::call(IndexMask mask,
                                   MFParams params,
                                   MFContext UNUSED(context)) const
{
  GenericMutableArrayRef r_value = params.uninitialized_single_output(0);
  r_value.type().fill_uninitialized_indices(m_value, r_value.buffer(), mask);
}

MF_GenericConstantVector::MF_GenericConstantVector(GenericArrayRef array) : m_array(array)
{
  const CPPType &type = array.type();
  MFSignatureBuilder signature = this->get_builder("Constant " + type.name() + " List");
  std::stringstream ss;
  ss << "[";
  uint max_amount = 5;
  for (uint i : IndexRange(std::min(max_amount, array.size()))) {
    MF_GenericConstantValue::value_to_string(ss, type, array[i]);
    ss << ", ";
  }
  if (max_amount < array.size()) {
    ss << "...";
  }
  ss << "]";
  signature.vector_output(ss.str(), type);
}

void MF_GenericConstantVector::call(IndexMask mask,
                                    MFParams params,
                                    MFContext UNUSED(context)) const
{
  GenericVectorArray &r_vectors = params.vector_output(0);
  for (uint i : mask) {
    r_vectors.extend_single__copy(i, m_array);
  }
}

}  // namespace FN
