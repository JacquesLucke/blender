/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DNA_volume_types.h"

#include "BKE_volume.h"
#include "BKE_volume_render.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Dense.h>
#  include <openvdb/tools/GridTransformer.h>
#endif

/* Dense Voxels */

template<typename GridType>
typename GridType::Ptr new_grid_with_changed_resolution(const openvdb::GridBase &old_grid,
                                                        const float resolution_factor)
{
  BLI_assert(resolution_factor > 0.0f);
  BLI_assert(old_grid.isType<GridType>());

  openvdb::Mat4R xform;
  xform.setToScale(openvdb::Vec3d(resolution_factor));
  openvdb::tools::GridTransformer transformer{xform};

  typename GridType::Ptr new_grid = GridType::create();
  transformer.transformGrid<openvdb::tools::BoxSampler>(static_cast<const GridType &>(old_grid),
                                                        *new_grid);
  new_grid->transform() = old_grid.transform();
  new_grid->transform().preScale(1.0f / resolution_factor);
  return new_grid;
}

bool BKE_volume_grid_dense_floats(const Volume *volume,
                                  VolumeGrid *volume_grid,
                                  const float resolution_factor,
                                  DenseFloatVolumeGrid *r_dense_grid)
{
  openvdb::GridBase::ConstPtr original_grid = BKE_volume_grid_openvdb_for_read(volume,
                                                                               volume_grid);
  openvdb::FloatGrid::Ptr downscaled_grid = new_grid_with_changed_resolution<openvdb::FloatGrid>(
      *original_grid, resolution_factor);
  openvdb::GridBase::ConstPtr grid = downscaled_grid;

  const openvdb::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();
  if (bbox.empty()) {
    return false;
  }
  const openvdb::Vec3i resolution = bbox.dim().asVec3i();

  const int64_t num_voxels = static_cast<int64_t>(resolution[0]) *
                             static_cast<int64_t>(resolution[1]) *
                             static_cast<int64_t>(resolution[2]);
  const int channels = BKE_volume_grid_channels(volume_grid);
  const int elem_size = sizeof(float) * channels;
  float *voxels = static_cast<float *>(MEM_malloc_arrayN(num_voxels, elem_size, __func__));
  if (voxels == NULL) {
    return false;
  }

  switch (BKE_volume_grid_type(volume_grid)) {
    case VOLUME_GRID_BOOLEAN: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::BoolGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_FLOAT: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_DOUBLE: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::DoubleGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_INT: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Int32Grid>(grid), dense);
      break;
    }
    case VOLUME_GRID_INT64: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Int64Grid>(grid), dense);
      break;
    }
    case VOLUME_GRID_MASK: {
      openvdb::tools::Dense<float, openvdb::tools::LayoutXYZ> dense(bbox, voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::MaskGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_VECTOR_FLOAT: {
      openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
          bbox, (openvdb::Vec3f *)voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3fGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_VECTOR_DOUBLE: {
      openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
          bbox, (openvdb::Vec3f *)voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3dGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_VECTOR_INT: {
      openvdb::tools::Dense<openvdb::Vec3f, openvdb::tools::LayoutXYZ> dense(
          bbox, (openvdb::Vec3f *)voxels);
      openvdb::tools::copyToDense(*openvdb::gridConstPtrCast<openvdb::Vec3IGrid>(grid), dense);
      break;
    }
    case VOLUME_GRID_STRING:
    case VOLUME_GRID_POINTS:
    case VOLUME_GRID_UNKNOWN: {
      /* Zero channels to copy. */
      break;
    }
  }

  r_dense_grid->voxels = voxels;
  r_dense_grid->channels = channels;
  copy_v3_v3_int(r_dense_grid->resolution, resolution.asV());

  {
    float index_to_object[4][4];
    memcpy(index_to_object,
           openvdb::Mat4s(grid->transform().baseMap()->getAffineMap()->getMat4()).asPointer(),
           sizeof(index_to_object));

    float texture_to_index[4][4];
    const openvdb::Vec3f loc = bbox.min().asVec3s();
    const openvdb::Vec3f size = bbox.dim().asVec3s();
    size_to_mat4(texture_to_index, size.asV());
    copy_v3_v3(texture_to_index[3], loc.asV());

    mul_m4_m4m4(r_dense_grid->texture_to_object, index_to_object, texture_to_index);
  }
  return true;
}

void BKE_volume_dense_float_grid_clear(DenseFloatVolumeGrid *dense_grid)
{
  if (dense_grid->voxels != NULL) {
    MEM_freeN(dense_grid->voxels);
  }
}

/* Wireframe */

#ifdef WITH_OPENVDB
struct VolumeWireframe {
  std::vector<openvdb::Vec3f> verts;
  std::vector<openvdb::Vec2I> edges;

  template<typename GridType>
  void add_grid(openvdb::GridBase::ConstPtr gridbase, const bool points, const bool coarse)
  {
    using TreeType = typename GridType::TreeType;
    using Depth2Type = typename TreeType::RootNodeType::ChildNodeType::ChildNodeType;
    using NodeCIter = typename TreeType::NodeCIter;
    using GridConstPtr = typename GridType::ConstPtr;

    GridConstPtr grid = openvdb::gridConstPtrCast<GridType>(gridbase);
    const openvdb::math::Transform &transform = grid->transform();
    const int depth = (coarse) ? 2 : 3;

    NodeCIter iter = grid->tree().cbeginNode();
    iter.setMaxDepth(depth);

    for (; iter; ++iter) {
      if (iter.getDepth() == depth) {
        openvdb::CoordBBox coordbbox;

        if (depth == 2) {
          /* Internal node at depth 2. */
          const Depth2Type *node = nullptr;
          iter.getNode(node);
          if (node) {
            node->evalActiveBoundingBox(coordbbox, false);
          }
          else {
            continue;
          }
        }
        else {
          /* Leaf node. */
          if (!iter.getBoundingBox(coordbbox)) {
            continue;
          }
        }

        /* +1 to convert from exclusive to include bounds. */
        coordbbox.max() = coordbbox.max().offsetBy(1);
        openvdb::BBoxd bbox = transform.indexToWorld(coordbbox);

        if (points) {
          add_point(bbox);
        }
        else {
          add_box(bbox);
        }
      }
    }
  }

  void add_point(const openvdb::BBoxd &bbox)
  {
    verts.push_back(bbox.getCenter());
  }

  void add_box(const openvdb::BBoxd &bbox)
  {
    /* TODO: deduplicate edges, hide flat edges? */
    openvdb::Vec3f min = bbox.min();
    openvdb::Vec3f max = bbox.max();

    const int vert_offset = verts.size();
    const int edge_offset = edges.size();

    /* Create vertices. */
    verts.resize(vert_offset + 8);
    verts[vert_offset + 0] = openvdb::Vec3f(min[0], min[1], min[2]);
    verts[vert_offset + 1] = openvdb::Vec3f(max[0], min[1], min[2]);
    verts[vert_offset + 2] = openvdb::Vec3f(max[0], max[1], min[2]);
    verts[vert_offset + 3] = openvdb::Vec3f(min[0], max[1], min[2]);
    verts[vert_offset + 4] = openvdb::Vec3f(min[0], min[1], max[2]);
    verts[vert_offset + 5] = openvdb::Vec3f(max[0], min[1], max[2]);
    verts[vert_offset + 6] = openvdb::Vec3f(max[0], max[1], max[2]);
    verts[vert_offset + 7] = openvdb::Vec3f(min[0], max[1], max[2]);

    /* Create edges. */
    const int box_edges[12][2] = {{0, 1},
                                  {1, 2},
                                  {2, 3},
                                  {3, 0},
                                  {4, 5},
                                  {5, 6},
                                  {6, 7},
                                  {7, 4},
                                  {0, 4},
                                  {1, 5},
                                  {2, 6},
                                  {3, 7}};

    edges.resize(edge_offset + 12);
    for (int i = 0; i < 12; i++) {
      edges[edge_offset + i] = openvdb::Vec2I(vert_offset + box_edges[i][0],
                                              vert_offset + box_edges[i][1]);
    }
  }
};
#endif

void BKE_volume_grid_wireframe(const Volume *volume,
                               VolumeGrid *volume_grid,
                               BKE_volume_wireframe_cb cb,
                               void *cb_userdata)
{
#ifdef WITH_OPENVDB
  VolumeWireframe wireframe;

  if (volume->display.wireframe_type == VOLUME_WIREFRAME_NONE) {
    /* Nothing. */
  }
  else if (volume->display.wireframe_type == VOLUME_WIREFRAME_BOUNDS) {
    /* Bounding box. */
    float min[3], max[3];
    BKE_volume_grid_bounds(volume_grid, min, max);

    openvdb::BBoxd bbox(min, max);
    wireframe.add_box(bbox);
  }
  else {
    /* Tree nodes. */
    openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
    const bool points = (volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS);
    const bool coarse = (volume->display.wireframe_detail == VOLUME_WIREFRAME_COARSE);

    switch (BKE_volume_grid_type(volume_grid)) {
      case VOLUME_GRID_BOOLEAN: {
        wireframe.add_grid<openvdb::BoolGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_FLOAT: {
        wireframe.add_grid<openvdb::FloatGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_DOUBLE: {
        wireframe.add_grid<openvdb::DoubleGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_INT: {
        wireframe.add_grid<openvdb::Int32Grid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_INT64: {
        wireframe.add_grid<openvdb::Int64Grid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_MASK: {
        wireframe.add_grid<openvdb::MaskGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_VECTOR_FLOAT: {
        wireframe.add_grid<openvdb::Vec3fGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_VECTOR_DOUBLE: {
        wireframe.add_grid<openvdb::Vec3dGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_VECTOR_INT: {
        wireframe.add_grid<openvdb::Vec3IGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_STRING: {
        wireframe.add_grid<openvdb::StringGrid>(grid, points, coarse);
        break;
      }
      case VOLUME_GRID_POINTS:
      case VOLUME_GRID_UNKNOWN: {
        break;
      }
    }
  }

  cb(cb_userdata,
     (float(*)[3])wireframe.verts.data(),
     (int(*)[2])wireframe.edges.data(),
     wireframe.verts.size(),
     wireframe.edges.size());
#else
  UNUSED_VARS(volume, volume_grid);
  cb(cb_userdata, NULL, NULL, 0, 0);
#endif
}

/* Render */

float BKE_volume_density_scale(const Volume *volume, const float matrix[4][4])
{
  if (volume->render.space == VOLUME_SPACE_OBJECT) {
    float unit[3] = {1.0f, 1.0f, 1.0f};
    normalize_v3(unit);
    mul_mat3_m4_v3(matrix, unit);
    return 1.0f / len_v3(unit);
  }

  return 1.0f;
}
