#pragma once

#include "parameter.hpp"

namespace FN {

class Signature {
 public:
  Signature() = default;
  ~Signature() = default;

  Signature(const InputParameters &inputs, const OutputParameters &outputs)
      : m_inputs(inputs), m_outputs(outputs)
  {
  }

  inline const InputParameters &inputs() const
  {
    return m_inputs;
  }

  inline const OutputParameters &outputs() const
  {
    return m_outputs;
  }

  TypeVector input_types() const;
  TypeVector output_types() const;

  bool has_interface(const TypeVector &inputs, const TypeVector &outputs) const;

  bool has_interface(const Signature &other) const;

 private:
  const InputParameters m_inputs;
  const OutputParameters m_outputs;
};

} /* namespace FN */
