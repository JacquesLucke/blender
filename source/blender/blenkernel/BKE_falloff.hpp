#pragma once

#include "BKE_attributes_ref.hpp"

namespace BKE {

class Falloff {
 public:
#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("BKE:Falloff")
#endif

  virtual ~Falloff();

  /**
   * Create an identical copy of this falloff.
   */
  virtual Falloff *clone() const = 0;

  /**
   * The indices are expected to be sorted. Also no index must exist more than once.
   */
  virtual void compute(AttributesRef attributes,
                       ArrayRef<uint> indices,
                       MutableArrayRef<float> r_weights) const = 0;
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
               MutableArrayRef<float> r_weights) const override;
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

  Falloff *clone() const override
  {
    return new PointDistanceFalloff(m_point, m_min_distance, m_max_distance);
  }

  void compute(AttributesRef attributes,
               ArrayRef<uint> indices,
               MutableArrayRef<float> r_weights) const override;
};

}  // namespace BKE
