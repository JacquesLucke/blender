#include "BKE_falloff.hpp"

namespace BKE {

Falloff::~Falloff()
{
}

void ConstantFalloff::compute(AttributesRef UNUSED(attributes),
                              ArrayRef<uint> indices,
                              MutableArrayRef<float> r_weights) const
{
  for (uint index : indices) {
    r_weights[index] = m_weight;
  }
}

void PointDistanceFalloff::compute(AttributesRef attributes,
                                   ArrayRef<uint> indices,
                                   MutableArrayRef<float> r_weights) const
{
  auto positions = attributes.get<float3>("Position");
  float distance_diff = m_max_distance - m_min_distance;

  for (uint index : indices) {
    float3 position = positions[index];
    float distance = float3::distance(position, m_point);

    float weight = 0;
    if (distance_diff > 0) {
      weight = 1.0f - (distance - m_min_distance) / distance_diff;
      CLAMP(weight, 0.0f, 1.0f);
    }
    r_weights[index] = weight;
  }
}

}  // namespace BKE
