#ifndef __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__
#define __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__

#include <mutex>

#include "FN_multi_function_context.h"
#include "FN_attributes_ref.h"

#include "BLI_math_cxx.h"
#include "BLI_map.h"

namespace FN {

using BLI::Map;

struct VertexPositionArray {
  ArrayRef<BLI::float3> positions;
};

struct SceneTimeContext {
  float time;
};

struct ParticleAttributesContext {
  AttributesRef attributes;
};

struct EmitterTimeInfoContext {
  float duration;
  float begin;
  float end;
  int step;
};

struct EventFilterEndTimeContext {
  float end_time;
};

struct EventFilterDurationsContext {
  ArrayRef<float> durations;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__ */
