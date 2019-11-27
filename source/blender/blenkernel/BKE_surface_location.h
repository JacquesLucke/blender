#ifndef __BKE_SURFACE_LOCATION_H__
#define __BKE_SURFACE_LOCATION_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * References a point on a surface. If the surface moves, the point moves with it. The surface is
 * identified by an integer.
 *
 * For now, only points on triangle meshes are supported, support for curves could be added too.
 */
typedef struct SurfaceLocation {
  /**
   * Identifies the surface that is being referenced. This is usually a hash of the name of an
   * object. The location is invalid, if this id is negative.
   */
  int32_t surface_id;

  /* Index of the triangle that contains the referenced location. */
  uint32_t triangle_index;

  /* Barycentric coordinates of the referenced location inside the triangle. */
  float weight1, weight2, weight3;

} SurfaceLocation;

#ifdef __cplusplus
}
#endif

#endif /* __BKE_SURFACE_LOCATION_H__ */
