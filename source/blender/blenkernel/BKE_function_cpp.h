#ifndef __BKE_FUNCTION_CPP_H__
#define __BKE_FUNCTION_CPP_H__

#include "BKE_type_cpp.h"
#include "BKE_tuple.h"

#include "BLI_vector.h"
#include "BLI_string_ref.h"

namespace BKE {

using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

class FunctionCPP;

struct SignatureData {
  std::string name;
  Vector<TypeCPP *> input_types;
  Vector<TypeCPP *> output_types;
  Vector<std::string> input_names;
  Vector<std::string> output_names;
};

class SignatureBuilderCPP {
 private:
  SignatureData m_data;

  friend FunctionCPP;

 public:
  void set_name(StringRef name)
  {
    m_data.name = name;
  }

  void add_input(StringRef name, TypeCPP &type)
  {
    m_data.input_names.append(name);
    m_data.input_types.append(&type);
  }

  void add_output(StringRef name, TypeCPP &type)
  {
    m_data.output_names.append(name);
    m_data.input_types.append(&type);
  }
};

class FunctionCPP {
 private:
  SignatureData m_signature;

 public:
  FunctionCPP();

  virtual ~FunctionCPP();

  StringRefNull name() const
  {
    return m_signature.name;
  }

  ArrayRef<TypeCPP *> input_types() const
  {
    return m_signature.input_types;
  }

  ArrayRef<TypeCPP *> output_types() const
  {
    return m_signature.output_types;
  }

  StringRefNull input_name(uint index) const
  {
    return m_signature.input_names[index];
  }

  StringRefNull output_name(uint index) const
  {
    return m_signature.output_names[index];
  }

  virtual void signature(SignatureBuilderCPP &signature) = 0;
  virtual void call(TupleRef &fn_in, TupleRef &fn_out) const = 0;
};

}  // namespace BKE

#endif /* __BKE_FUNCTION_CPP_H__ */