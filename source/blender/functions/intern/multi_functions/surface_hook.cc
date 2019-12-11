#include "surface_hook.h"

#include "BKE_surface_hook.h"
#include "BKE_id_handle.h"
#include "BKE_id_data_cache.h"
#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"
#include "BKE_customdata.h"
#include "BKE_image.h"

#include "IMB_imbuf_types.h"

#include "DNA_customdata_types.h"

#include "BLI_math_cxx.h"

#include "util.h"

namespace FN {

using BKE::IDDataCache;
using BKE::IDHandleLookup;
using BKE::ImageIDHandle;
using BKE::ObjectIDHandle;
using BKE::SurfaceHook;
using BLI::float2;
using BLI::float3;
using BLI::float4x4;
using BLI::rgba_b;
using BLI::rgba_f;

MF_ClosestSurfaceHookOnObject::MF_ClosestSurfaceHookOnObject()
{
  MFSignatureBuilder signature("Closest Point on Object");
  signature.single_input<ObjectIDHandle>("Object");
  signature.single_input<float3>("Position");
  signature.single_output<SurfaceHook>("Closest Location");
  this->set_signature(signature);
}

static BVHTreeNearest get_nearest_point(BVHTreeFromMesh *bvhtree_data, float3 point)
{
  BVHTreeNearest nearest = {0};
  nearest.dist_sq = 10000000.0f;
  nearest.index = -1;
  BLI_bvhtree_find_nearest(
      bvhtree_data->tree, point, &nearest, bvhtree_data->nearest_callback, (void *)bvhtree_data);
  return nearest;
}

static float3 get_barycentric_coords(Mesh *mesh,
                                     const MLoopTri *triangles,
                                     float3 position,
                                     uint triangle_index)
{
  const MLoopTri &triangle = triangles[triangle_index];

  float3 v1 = mesh->mvert[mesh->mloop[triangle.tri[0]].v].co;
  float3 v2 = mesh->mvert[mesh->mloop[triangle.tri[1]].v].co;
  float3 v3 = mesh->mvert[mesh->mloop[triangle.tri[2]].v].co;

  float3 weights;
  interp_weights_tri_v3(weights, v1, v2, v3, position);
  return weights;
}

void MF_ClosestSurfaceHookOnObject::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<ObjectIDHandle> object_handles = params.readonly_single_input<ObjectIDHandle>(
      0, "Object");
  VirtualListRef<float3> positions = params.readonly_single_input<float3>(1, "Position");
  MutableArrayRef<SurfaceHook> r_surface_hooks = params.uninitialized_single_output<SurfaceHook>(
      2, "Closest Location");

  auto *id_data_cache = context.try_find_global<BKE::IDDataCache>();
  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();

  if (id_data_cache == nullptr || id_handle_lookup == nullptr) {
    r_surface_hooks.fill_indices(mask.indices(), {});
    return;
  }

  group_indices_by_same_value(
      mask.indices(),
      object_handles,
      [&](ObjectIDHandle object_handle, ArrayRef<uint> indices_with_same_object) {
        Object *object = id_handle_lookup->lookup(object_handle);
        if (object == nullptr) {
          r_surface_hooks.fill_indices(indices_with_same_object, {});
          return;
        }

        BVHTreeFromMesh *bvhtree = id_data_cache->get_bvh_tree(object);
        if (bvhtree == nullptr) {
          r_surface_hooks.fill_indices(indices_with_same_object, {});
          return;
        }

        Mesh *mesh = (Mesh *)object->data;
        const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);

        float4x4 global_to_local = float4x4(object->obmat).inverted__LocRotScale();

        for (uint i : indices_with_same_object) {
          float3 local_position = global_to_local.transform_position(positions[i]);
          BVHTreeNearest nearest = get_nearest_point(bvhtree, local_position);
          if (nearest.index == -1) {
            r_surface_hooks[i] = {};
            continue;
          }

          float3 bary_coords = get_barycentric_coords(mesh, triangles, nearest.co, nearest.index);
          r_surface_hooks[i] = SurfaceHook(object_handle, nearest.index, bary_coords);
        }
      });
}

MF_GetPositionOnSurface::MF_GetPositionOnSurface()
{
  MFSignatureBuilder signature("Get Position on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_output<float3>("Position");
  this->set_signature(signature);
}

void MF_GetPositionOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<SurfaceHook> surface_hooks = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  MutableArrayRef<float3> r_positions = params.uninitialized_single_output<float3>(1, "Position");

  float3 fallback = {0, 0, 0};

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_positions.fill_indices(mask.indices(), fallback);
    return;
  }

  group_indices_by_same_value(
      mask.indices(),
      surface_hooks,
      [&](SurfaceHook base_hook, ArrayRef<uint> indices_on_same_surface) {
        if (base_hook.type() != BKE::SurfaceHookType::MeshObject) {
          r_positions.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Object *object = id_handle_lookup->lookup(base_hook.object_handle());
        if (object == nullptr) {
          r_positions.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Mesh *mesh = (Mesh *)object->data;
        const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
        int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

        for (uint i : indices_on_same_surface) {
          SurfaceHook hook = surface_hooks[i];

          if (hook.triangle_index() >= triangle_amount) {
            r_positions[i] = fallback;
            continue;
          }

          const MLoopTri &triangle = triangles[hook.triangle_index()];
          float3 v1 = mesh->mvert[mesh->mloop[triangle.tri[0]].v].co;
          float3 v2 = mesh->mvert[mesh->mloop[triangle.tri[1]].v].co;
          float3 v3 = mesh->mvert[mesh->mloop[triangle.tri[2]].v].co;

          float3 position;
          interp_v3_v3v3v3(position, v1, v2, v3, hook.bary_coords());
          float4x4 local_to_world = object->obmat;
          position = local_to_world.transform_position(position);

          r_positions[i] = position;
        }
      },
      SurfaceHook::on_same_surface);
}

MF_GetNormalOnSurface::MF_GetNormalOnSurface()
{
  MFSignatureBuilder signature("Get Normal on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_output<float3>("Normal");
  this->set_signature(signature);
}

static float3 short_normal_to_float3(const short normal[3])
{
  return float3(
      (float)normal[0] / 32767.0f, (float)normal[1] / 32767.0f, (float)normal[2] / 32767.0f);
}

void MF_GetNormalOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<SurfaceHook> surface_hooks = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  MutableArrayRef<float3> r_normals = params.uninitialized_single_output<float3>(1, "Normal");

  float3 fallback = {0, 0, 1};

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_normals.fill_indices(mask.indices(), fallback);
    return;
  }

  group_indices_by_same_value(
      mask.indices(),
      surface_hooks,
      [&](SurfaceHook base_hook, ArrayRef<uint> indices_on_same_surface) {
        if (base_hook.type() != BKE::SurfaceHookType::MeshObject) {
          r_normals.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Object *object = id_handle_lookup->lookup(base_hook.object_handle());
        if (object == nullptr) {
          r_normals.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Mesh *mesh = (Mesh *)object->data;
        const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
        int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

        for (uint i : indices_on_same_surface) {
          SurfaceHook hook = surface_hooks[i];

          if (hook.triangle_index() >= triangle_amount) {
            r_normals[i] = fallback;
            continue;
          }

          const MLoopTri &triangle = triangles[hook.triangle_index()];
          float3 v1 = short_normal_to_float3(mesh->mvert[mesh->mloop[triangle.tri[0]].v].no);
          float3 v2 = short_normal_to_float3(mesh->mvert[mesh->mloop[triangle.tri[1]].v].no);
          float3 v3 = short_normal_to_float3(mesh->mvert[mesh->mloop[triangle.tri[2]].v].no);

          float3 position;
          interp_v3_v3v3v3(position, v1, v2, v3, hook.bary_coords());
          float4x4 local_to_world = object->obmat;
          position = local_to_world.transform_direction(position);

          r_normals[i] = position;
        }
      },
      SurfaceHook::on_same_surface);
}

MF_GetWeightOnSurface::MF_GetWeightOnSurface(std::string vertex_group_name)
    : m_vertex_group_name(std::move(vertex_group_name))
{
  MFSignatureBuilder signature("Get Weight on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_output<float>("Weight");
  this->set_signature(signature);
}

void MF_GetWeightOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<SurfaceHook> surface_hooks = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  MutableArrayRef<float> r_weights = params.uninitialized_single_output<float>(1, "Weight");

  float fallback = 0.0f;

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_weights.fill_indices(mask.indices(), fallback);
    return;
  }

  group_indices_by_same_value(
      mask.indices(),
      surface_hooks,
      [&](SurfaceHook base_hook, ArrayRef<uint> indices_on_same_surface) {
        if (base_hook.type() != BKE::SurfaceHookType::MeshObject) {
          r_weights.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Object *object = id_handle_lookup->lookup(base_hook.object_handle());
        if (object == nullptr) {
          r_weights.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Mesh *mesh = (Mesh *)object->data;
        const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
        int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

        MDeformVert *vertex_weights = mesh->dvert;
        int group_index = defgroup_name_index(object, m_vertex_group_name.data());
        if (group_index == -1 || vertex_weights == nullptr) {
          r_weights.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        for (uint i : indices_on_same_surface) {
          SurfaceHook hook = surface_hooks[i];

          if (hook.triangle_index() >= triangle_amount) {
            r_weights[i] = fallback;
            continue;
          }

          const MLoopTri &triangle = triangles[hook.triangle_index()];
          uint v1 = mesh->mloop[triangle.tri[0]].v;
          uint v2 = mesh->mloop[triangle.tri[1]].v;
          uint v3 = mesh->mloop[triangle.tri[2]].v;

          float3 corner_weights{defvert_find_weight(vertex_weights + v1, group_index),
                                defvert_find_weight(vertex_weights + v2, group_index),
                                defvert_find_weight(vertex_weights + v3, group_index)};

          float weight = float3::dot(hook.bary_coords(), corner_weights);
          r_weights[i] = weight;
        }
      },
      SurfaceHook::on_same_surface);
}

MF_GetImageColorOnSurface::MF_GetImageColorOnSurface()
{
  MFSignatureBuilder signature("Get Image Color on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_input<ImageIDHandle>("Image");
  signature.single_output<rgba_f>("Color");
  this->set_signature(signature);
}

static void get_colors_on_surface(ArrayRef<uint> indices,
                                  VirtualListRef<SurfaceHook> surface_hooks,
                                  MutableArrayRef<rgba_f> r_colors,
                                  rgba_f fallback,
                                  const IDHandleLookup &id_handle_lookup,
                                  const ImBuf &ibuf)
{
  group_indices_by_same_value(
      indices,
      surface_hooks,
      [&](SurfaceHook base_hook, ArrayRef<uint> indices_on_same_surface) {
        if (base_hook.type() != BKE::SurfaceHookType::MeshObject) {
          r_colors.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Object *object = id_handle_lookup.lookup(base_hook.object_handle());
        if (object == nullptr) {
          r_colors.fill_indices(indices_on_same_surface, fallback);
          return;
        }

        Mesh *mesh = (Mesh *)object->data;
        const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
        int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

        int uv_layer_index = 0;
        ArrayRef<MLoopUV> uv_layer = BLI::ref_c_array(
            (MLoopUV *)CustomData_get_layer_n(&mesh->ldata, CD_MLOOPUV, uv_layer_index),
            mesh->totloop);

        ArrayRef<rgba_b> pixel_buffer = BLI::ref_c_array((rgba_b *)ibuf.rect, ibuf.x * ibuf.y);

        for (uint i : indices_on_same_surface) {
          SurfaceHook hook = surface_hooks[i];
          if (hook.triangle_index() >= triangle_amount) {
            r_colors[i] = fallback;
            continue;
          }

          const MLoopTri &triangle = triangles[hook.triangle_index()];

          float2 uv1 = uv_layer[triangle.tri[0]].uv;
          float2 uv2 = uv_layer[triangle.tri[1]].uv;
          float2 uv3 = uv_layer[triangle.tri[2]].uv;

          float2 uv;
          interp_v2_v2v2v2(uv, uv1, uv2, uv3, hook.bary_coords());

          uv = uv.clamped_01();
          uint x = uv.x * (ibuf.x - 1);
          uint y = uv.y * (ibuf.y - 1);
          rgba_b color = pixel_buffer[y * ibuf.x + x];
          r_colors[i] = color;
        }
      },
      SurfaceHook::on_same_surface);
}

void MF_GetImageColorOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  if (mask.indices_amount() == 0) {
    return;
  }

  VirtualListRef<SurfaceHook> surface_hooks = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  VirtualListRef<ImageIDHandle> image_handles = params.readonly_single_input<ImageIDHandle>(
      1, "Image");
  MutableArrayRef<rgba_f> r_colors = params.uninitialized_single_output<rgba_f>(2, "Color");

  rgba_f fallback = {0.0f, 0.0f, 0.0f, 1.0f};

  auto *id_handle_lookup = context.try_find_global<IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_colors.fill_indices(mask.indices(), fallback);
    return;
  }

  group_indices_by_same_value<ImageIDHandle>(
      mask.indices(),
      image_handles,
      [&](ImageIDHandle image_handle, ArrayRef<uint> indices_with_image) {
        Image *image = id_handle_lookup->lookup(image_handle);
        if (image == nullptr) {
          r_colors.fill_indices(indices_with_image, fallback);
          return;
        }

        ImageUser image_user = {0};
        image_user.ok = true;
        ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, NULL);

        get_colors_on_surface(
            indices_with_image, surface_hooks, r_colors, fallback, *id_handle_lookup, *ibuf);

        BKE_image_release_ibuf(image, ibuf, NULL);
      });
}

}  // namespace FN
