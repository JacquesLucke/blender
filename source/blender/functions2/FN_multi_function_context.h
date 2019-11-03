#ifndef __FN_MULTI_FUNCTION_CONTEXT_H__
#define __FN_MULTI_FUNCTION_CONTEXT_H__

#include "BLI_math_cxx.h"

namespace FN {

class MFContext {
 public:
  ArrayRef<BLI::float3> vertex_positions;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_CONTEXT_H__ */
