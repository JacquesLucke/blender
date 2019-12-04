#ifndef __BKE_SURFACE_LOCATION_H__
#define __BKE_SURFACE_LOCATION_H__

#include "BLI_utildefines.h"
#include "BLI_math_cxx.h"

#include "BKE_id_handle.h"

namespace BKE {

using BLI::float3;

namespace SurfaceLocationType {
enum Enum {
  None,
  MeshObject,
};
}

/**
 * References a point on a surface. If the surface moves, the point moves with it.
 */
class SurfaceLocation {
 private:
  SurfaceLocationType::Enum m_type;

  /**
   * Used to identify the object if m_type is MeshObject.
   */
  ObjectIDHandle m_object_handle;

  /* Index of the triangle that contains the referenced location. */
  uint32_t m_triangle_index;

  /* Barycentric coordinates of the referenced location inside the triangle. */
  float3 m_bary_coords;

 public:
  SurfaceLocation() : m_type(SurfaceLocationType::None)
  {
  }

  SurfaceLocation(ObjectIDHandle object_handle, uint32_t triangle_index, float3 bary_coords)
      : m_type(SurfaceLocationType::MeshObject),
        m_object_handle(object_handle),
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

  ObjectIDHandle object_handle() const
  {
    BLI_assert(m_type == SurfaceLocationType::MeshObject);
    return m_object_handle;
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
};

}  // namespace BKE

#endif /* __BKE_SURFACE_LOCATION_H__ */
