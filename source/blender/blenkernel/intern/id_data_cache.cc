#include "BKE_id_data_cache.h"

namespace BKE {

IDDataCache::~IDDataCache()
{
  for (auto bvhtree : m_bvh_trees.values()) {
    if (bvhtree != nullptr) {
      free_bvhtree_from_mesh(bvhtree);
      delete bvhtree;
    }
  }
}

BVHTreeFromMesh *IDDataCache::get_bvh_tree(Object *object) const
{
  BLI_assert(object != nullptr);

  std::lock_guard<std::mutex> lock(m_bvt_trees_mutex);

  return m_bvh_trees.lookup_or_add(object, [&]() -> BVHTreeFromMesh * {
    if (object->type != OB_MESH) {
      return nullptr;
    }
    BVHTreeFromMesh *bvhtree_data = new BVHTreeFromMesh();
    BKE_bvhtree_from_mesh_get(bvhtree_data, (Mesh *)object->data, BVHTREE_FROM_LOOPTRI, 2);
    return bvhtree_data;
  });
}

}  // namespace BKE
