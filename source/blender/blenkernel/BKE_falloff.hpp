#pragma once

#include "BKE_attributes_ref.hpp"
#include "BKE_bvhutils.h"

#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "DNA_object_types.h"

namespace BKE {

using BLI::float4x4;

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

  Falloff *clone() const override
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

class MeshDistanceFalloff : public Falloff {
 private:
  Object *m_object;
  BVHTreeFromMesh m_bvhtree_data;
  float4x4 m_local_to_world;
  float4x4 m_world_to_local;
  float m_inner_distance;
  float m_outer_distance;

 public:
  MeshDistanceFalloff(Object *object, float inner_distance, float outer_distance);
  ~MeshDistanceFalloff();

  Falloff *clone() const override
  {
    return new MeshDistanceFalloff(m_object, m_inner_distance, m_outer_distance);
  }

  void compute(AttributesRef attributes,
               ArrayRef<uint> indices,
               MutableArrayRef<float> r_weights) const override;
};

}  // namespace BKE
