#ifndef __BKE_SURFACE_HOOK_H__
#define __BKE_SURFACE_HOOK_H__

#include "BLI_utildefines.h"
#include "BLI_math_cxx.h"

#include "BKE_id_handle.h"

namespace BKE {

using BLI::float3;

namespace SurfaceHookType {
enum Enum {
  None,
  MeshObject,
};
}

/**
 * References a point on a surface. If the surface moves, the point moves with it.
 */
class SurfaceHook {
 private:
  SurfaceHookType::Enum m_type;

  /**
   * Used to identify the object if m_type is MeshObject.
   */
  ObjectIDHandle m_object_handle;

  /* Index of the triangle that contains the referenced location. */
  uint32_t m_triangle_index;

  /* Barycentric coordinates of the referenced location inside the triangle. */
  float3 m_bary_coords;

 public:
  SurfaceHook() : m_type(SurfaceHookType::None)
  {
  }

  SurfaceHook(ObjectIDHandle object_handle, uint32_t triangle_index, float3 bary_coords)
      : m_type(SurfaceHookType::MeshObject),
        m_object_handle(object_handle),
        m_triangle_index(triangle_index),
        m_bary_coords(bary_coords)
  {
  }

  SurfaceHookType::Enum type() const
  {
    return m_type;
  }

  bool is_valid() const
  {
    return m_type != SurfaceHookType::None;
  }

  ObjectIDHandle object_handle() const
  {
    BLI_assert(m_type == SurfaceHookType::MeshObject);
    return m_object_handle;
  }

  uint32_t triangle_index() const
  {
    BLI_assert(m_type == SurfaceHookType::MeshObject);
    return m_triangle_index;
  }

  float3 bary_coords() const
  {
    BLI_assert(m_type == SurfaceHookType::MeshObject);
    return m_bary_coords;
  }

  static bool on_same_surface(const SurfaceHook &a, const SurfaceHook &b)
  {
    if (a.type() != b.type()) {
      return false;
    }
    switch (a.type()) {
      case BKE::SurfaceHookType::None:
        return true;
      case BKE::SurfaceHookType::MeshObject:
        return a.object_handle() == b.object_handle();
    }
    BLI_assert(false);
    return false;
  }
};

}  // namespace BKE

#endif /* __BKE_SURFACE_HOOK_H__ */
