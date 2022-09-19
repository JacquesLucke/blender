/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_math_vector.hh"
#include "BLI_span.hh"

#include "GPU_batch.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "draw_cache_extract.hh"
#include "draw_cache_impl.h"
#include "overlay_private.hh"

void OVERLAY_attribute_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
                         DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->attribute_ps, state | pd->clipping_state);

  GPUShader *mesh_sh = OVERLAY_shader_attribute_mesh();
  GPUShader *point_cloud_sh = OVERLAY_shader_attribute_point_cloud();
  GPUShader *curve_sh = OVERLAY_shader_attribute_curve();
  GPUShader *curves_sh = OVERLAY_shader_attribute_curves();
  pd->attribute_mesh_grp = DRW_shgroup_create(mesh_sh, psl->attribute_ps);
  pd->attribute_pointcloud_grp = DRW_shgroup_create(point_cloud_sh, psl->attribute_ps);
  pd->attribute_curve_grp = DRW_shgroup_create(curve_sh, psl->attribute_ps);
  pd->attribute_curves_grp = DRW_shgroup_create(curves_sh, psl->attribute_ps);
}

void OVERLAY_attribute_cache_populate(OVERLAY_Data *vedata, Object *object)
{
  using namespace blender;

  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (object->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(object->data);
    if (mesh->attributes().contains(".viewer")) {
      GPUBatch *batch = DRW_cache_mesh_surface_attribute_get(object);
      DRW_shgroup_call(pd->attribute_mesh_grp, batch, object);
    }
  }
  else if (object->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = static_cast<PointCloud *>(object->data);
    if (pointcloud->attributes().contains(".viewer")) {
      GPUBatch *batch = DRW_cache_pointcloud_surface_attribute_get(object);
      DRW_shgroup_call_instance_range(pd->attribute_pointcloud_grp, object, batch, 0, 0);
    }
  }
  else if (object->type == OB_CURVES_LEGACY) {
    Curve *curve = static_cast<Curve *>(object->data);
    const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curve->curve_eval->geometry);
    if (curves.attributes().contains(".viewer")) {
      GPUBatch *batch = DRW_cache_curve_edge_write_attribute_get(object);
      DRW_shgroup_call(pd->attribute_curve_grp, batch, object);
    }
  }
  else if (object->type == OB_CURVES) {
    Curves *curves_id = static_cast<Curves *>(object->data);
    const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id->geometry);
    if (curves.attributes().contains(".viewer")) {
      bool is_point_domain;
      GPUTexture **texture = DRW_curves_texture_for_evaluated_attribute(
          curves_id, ".viewer", &is_point_domain);
      DRWShadingGroup *grp = DRW_shgroup_curves_create_sub(
          object, pd->attribute_curves_grp, nullptr);
      DRW_shgroup_uniform_bool_copy(grp, "is_point_domain", is_point_domain);
      DRW_shgroup_uniform_texture(grp, "color_tx", *texture);
    }
  }
}

void OVERLAY_attribute_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(pd->painting.in_front ? dfbl->in_front_fb : dfbl->default_fb);
  }

  DRW_draw_pass(psl->attribute_ps);
}
