#ifndef __BKE_SURFACE_LOCATION_H__
#define __BKE_SURFACE_LOCATION_H__

#include "BLI_utildefines.h"
#include "BLI_math_cxx.h"

#include "DNA_object_types.h"

#include "BLI_hash.h"

namespace BKE {

using BLI::float3;

/**
 * References a point on a surface. If the surface moves, the point moves with it. The surface is
 * identified by an integer.
 *
 * For now, only points on triangle meshes are supported, support for curves could be added too.
 */
struct SurfaceLocation {
  /**
   * Identifies the surface that is being referenced. This is usually a hash of the name of an
   * object. The location is invalid, if this id is negative.
   */
  int32_t surface_id;

  /* Index of the triangle that contains the referenced location. */
  uint32_t triangle_index;

  /* Barycentric coordinates of the referenced location inside the triangle. */
  float3 bary_coords;

  SurfaceLocation() : surface_id(-1)
  {
  }

  SurfaceLocation(int32_t surface_id, uint32_t triangle_index, float3 bary_coords)
      : surface_id(surface_id), triangle_index(triangle_index), bary_coords(bary_coords)
  {
  }

  static int32_t ComputeObjectSurfaceID(const Object *ob)
  {
    BLI_assert(ob != nullptr);

    /* Set the highest bit to zero, to make the number positive. */
    return BLI_hash_string(ob->id.name) & ~(1 << 31);
  }
};

}  // namespace BKE

#endif /* __BKE_SURFACE_LOCATION_H__ */
