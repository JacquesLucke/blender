#ifndef __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__
#define __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__

#include "FN_multi_function_context.h"

#include "BLI_math_cxx.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

namespace FN {

class VertexPositionArray : public MFElementContext {
 public:
  ArrayRef<BLI::float3> positions;
};

class SceneTimeContext : public MFElementContext {
 public:
  float time;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__ */
