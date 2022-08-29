/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "draw_cache_impl.h"
#include "overlay_private.h"

void OVERLAY_attribute_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->attribute_ps, state | pd->clipping_state);

  GPUShader *sh = OVERLAY_shader_attribute();
  pd->attribute_grp = DRW_shgroup_create(sh, psl->attribute_ps);
}

void OVERLAY_attribute_cache_populate(OVERLAY_Data *vedata, Object *object)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  Mesh *mesh = static_cast<Mesh *>(object->data);

  GPUBatch *batch = DRW_mesh_batch_cache_get_surface(mesh);
  DRW_shgroup_call(pd->attribute_grp, batch, object);
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
