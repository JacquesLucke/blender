#ifndef __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__
#define __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__

#include <mutex>

#include "FN_multi_function_context.h"
#include "FN_attributes_ref.h"

#include "BLI_math_cxx.h"
#include "BLI_map.h"
#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"

#include "BKE_bvhutils.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

namespace FN {

using BLI::Map;

class VertexPositionArray : public MFElementContext {
 public:
  ArrayRef<BLI::float3> positions;
};

class SceneTimeContext : public MFElementContext {
 public:
  float time;
};

class ParticleAttributesContext : public MFElementContext {
 public:
  AttributesRef attributes;

  ParticleAttributesContext(AttributesRef attributes) : attributes(attributes)
  {
  }
};

class PersistentSurfacesLookupContext : public MFElementContext {
 private:
  Map<int32_t, Object *> m_object_by_id;

 public:
  PersistentSurfacesLookupContext(Map<int32_t, Object *> object_by_id)
      : m_object_by_id(object_by_id)
  {
  }

  Object *lookup(int32_t id) const
  {
    return m_object_by_id.lookup_default(id, nullptr);
  }
};

class ExternalDataCacheContext : public MFElementContext {
 private:
  mutable Map<Object *, BVHTreeFromMesh *> m_bvh_trees;
  mutable std::mutex m_bvt_trees_mutex;

 public:
  ExternalDataCacheContext() = default;

  ~ExternalDataCacheContext()
  {
    for (auto bvhtree : m_bvh_trees.values()) {
      if (bvhtree != nullptr) {
        free_bvhtree_from_mesh(bvhtree);
        delete bvhtree;
      }
    }
  }

  BVHTreeFromMesh *get_bvh_tree(Object *object) const
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
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_COMMON_CONTEXTS_H__ */
