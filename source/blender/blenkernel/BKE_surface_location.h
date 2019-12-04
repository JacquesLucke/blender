#ifndef __BKE_SURFACE_LOCATION_H__
#define __BKE_SURFACE_LOCATION_H__

#include "BLI_utildefines.h"
#include "BLI_math_cxx.h"

#include "DNA_object_types.h"

#include "BLI_hash.h"

namespace BKE {

using BLI::float3;

namespace SurfaceLocationType {
enum Enum {
  None,
  MeshObject,
};
}

/**
 * References a point on a surface. If the surface moves, the point moves with it. The surface is
 * identified by an integer.
 *
 * For now, only points on triangle meshes are supported, support for curves could be added too.
 */
class SurfaceLocation {
 private:
  SurfaceLocationType::Enum m_type;

  /**
   * Identifies the surface that is being referenced. This can e.g. be a hash of the name of an
   * object.
   */
  uint32_t m_surface_id;

  /* Index of the triangle that contains the referenced location. */
  uint32_t m_triangle_index;

  /* Barycentric coordinates of the referenced location inside the triangle. */
  float3 m_bary_coords;

 public:
  SurfaceLocation() : m_type(SurfaceLocationType::None)
  {
  }

  SurfaceLocation(uint32_t surface_id, uint32_t triangle_index, float3 bary_coords)
      : m_type(SurfaceLocationType::MeshObject),
        m_surface_id(surface_id),
        m_triangle_index(triangle_index),
        m_bary_coords(bary_coords)
  {
  }

  SurfaceLocationType::Enum type() const
  {
    return m_type;
  }

  bool is_valid() const
  {
    return m_type != SurfaceLocationType::None;
  }

  uint32_t surface_id() const
  {
    BLI_assert(this->is_valid());
    return m_surface_id;
  }

  uint32_t triangle_index() const
  {
    BLI_assert(m_type == SurfaceLocationType::MeshObject);
    return m_triangle_index;
  }

  float3 bary_coords() const
  {
    BLI_assert(m_type == SurfaceLocationType::MeshObject);
    return m_bary_coords;
  }

  static uint32_t ComputeObjectSurfaceID(const Object *ob)
  {
    BLI_assert(ob != nullptr);

    return BLI_hash_string(ob->id.name);
  }
};

}  // namespace BKE

#endif /* __BKE_SURFACE_LOCATION_H__ */
