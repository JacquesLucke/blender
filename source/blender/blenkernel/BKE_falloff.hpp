#pragma once

#include "BKE_attributes_ref.hpp"

namespace BKE {

class Falloff {
 public:
  virtual ~Falloff()
  {
  }
  virtual Falloff *clone() const = 0;

  /**
   * The indices are expected to be sorted. Also no index must exist more than once.
   */
  virtual void compute(AttributesRef attributes,
                       ArrayRef<uint> indices,
                       MutableArrayRef<float> r_weights) = 0;
};

class ConstantFalloff : public Falloff {
 private:
  float m_weight;

 public:
  ConstantFalloff(float weight) : m_weight(weight)
  {
  }

  Falloff *clone() const
  {
    return new ConstantFalloff(m_weight);
  }

  void compute(AttributesRef UNUSED(attributes),
               ArrayRef<uint> indices,
               MutableArrayRef<float> r_weights)
  {
    for (uint index : indices) {
      r_weights[index] = m_weight;
    }
  }
};

class PointDistanceFalloff : public Falloff {
 private:
  float3 m_point;
  float m_min_distance;
  float m_max_distance;

 public:
  PointDistanceFalloff(float3 point, float min_distance, float max_distance)
      : m_point(point), m_min_distance(min_distance), m_max_distance(max_distance)
  {
  }

  Falloff *clone() const
  {
    return new PointDistanceFalloff(m_point, m_min_distance, m_max_distance);
  }

  void compute(AttributesRef attributes, ArrayRef<uint> indices, MutableArrayRef<float> r_weights)
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
};

}  // namespace BKE
