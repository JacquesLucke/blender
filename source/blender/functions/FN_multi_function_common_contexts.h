#ifndef __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__
#define __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__

#include <mutex>

#include "FN_multi_function_context.h"
#include "FN_attributes_ref.h"

#include "BLI_math_cxx.h"
#include "BLI_map.h"

namespace FN {

using BLI::Map;

class VertexPositionArray {
 public:
  ArrayRef<BLI::float3> positions;
};

class SceneTimeContext {
 public:
  float time;
};

class ParticleAttributesContext {
 public:
  AttributesRef attributes;

  ParticleAttributesContext(AttributesRef attributes) : attributes(attributes)
  {
  }
};

class EmitterTimeInfoContext {
 public:
  float duration;
  float begin;
  float end;
  int step;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__ */
