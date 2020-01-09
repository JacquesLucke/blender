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

}  // namespace FN
