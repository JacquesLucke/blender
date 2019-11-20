#pragma once

#include "FN_multi_function.h"

#include "vnode_multi_function_wrapper.h"
#include "mappings.h"

namespace FN {

class VTreeMultiFunction final : public MultiFunction {
 private:
  Vector<const VOutputSocket *> m_inputs;
  Vector<const VInputSocket *> m_outputs;

 public:
  VTreeMultiFunction(Vector<const VOutputSocket *> inputs, Vector<const VInputSocket *> outputs)
      : m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
  }

  void call(MFMask UNUSED(mask), MFParams UNUSED(params), MFContext UNUSED(context)) const override
  {
  }
};

}  // namespace FN
