#ifndef __BKE_ID_DATA_CACHE_H__
#define __BKE_ID_DATA_CACHE_H__

#include <mutex>

#include "BLI_map.h"
#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_bvhutils.h"

namespace BKE {

using BLI::Map;

class IDDataCache {
 private:
  mutable Map<Object *, BVHTreeFromMesh *> m_bvh_trees;
  mutable std::mutex m_bvt_trees_mutex;

 public:
  IDDataCache() = default;
  ~IDDataCache();

  BVHTreeFromMesh *get_bvh_tree(Object *object) const;
};

}  // namespace BKE

#endif /* __BKE_ID_DATA_CACHE_H__ */
