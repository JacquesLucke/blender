#ifndef __BKE_MULTI_FUNCTIONS_H__
#define __BKE_MULTI_FUNCTIONS_H__

#include "BKE_multi_function.h"

namespace BKE {

class MultiFunction_AddFloats final : public MultiFunction {
  void signature(Signature &signature) const override;
  void call(ArrayRef<uint> mask_indices, Params &params) const override;
};

};  // namespace BKE

#endif /* __BKE_MULTI_FUNCTIONS_H__ */
