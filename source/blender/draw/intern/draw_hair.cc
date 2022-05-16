/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_duplilist.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#include "draw_hair_private.h"
#include "draw_shader.h"

#ifndef __APPLE__
#  define USE_TRANSFORM_FEEDBACK
#  define USE_COMPUTE_SHADERS
#endif

BLI_INLINE eParticleRefineShaderType drw_hair_shader_type_get()
{
#ifdef USE_COMPUTE_SHADERS
  if (GPU_compute_shader_support() && GPU_shader_storage_buffer_objects_support()) {
    return PART_REFINE_SHADER_COMPUTE;
  }
#endif
#ifdef USE_TRANSFORM_FEEDBACK
  return PART_REFINE_SHADER_TRANSFORM_FEEDBACK;
#endif
  return PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND;
}

#ifndef USE_TRANSFORM_FEEDBACK
struct ParticleRefineCall {
  struct ParticleRefineCall *next;
  GPUVertBuf *vbo;
  DRWShadingGroup *shgrp;
  uint vert_len;
};

static ParticleRefineCall *g_tf_calls = nullptr;
static int g_tf_id_offset;
static int g_tf_target_width;
static int g_tf_target_height;
#endif

static GPUVertBuf *g_dummy_vbo = nullptr;
static GPUTexture *g_dummy_texture = nullptr;
static DRWPass *g_tf_pass; /* XXX can be a problem with multiple DRWManager in the future */

void DRW_hair_init(void)
{
#if defined(USE_TRANSFORM_FEEDBACK) || defined(USE_COMPUTE_SHADERS)
  g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_NO_DRAW);
#else
  g_tf_pass = DRW_pass_create("Update Hair Pass", DRW_STATE_WRITE_COLOR);
#endif

  if (g_dummy_vbo == nullptr) {
    /* initialize vertex format */
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    g_dummy_vbo = GPU_vertbuf_create_with_format(&format);

    const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(g_dummy_vbo, 1);
    GPU_vertbuf_attr_fill(g_dummy_vbo, dummy_id, vert);
    /* Create vbo immediately to bind to texture buffer. */
    GPU_vertbuf_use(g_dummy_vbo);

    g_dummy_texture = GPU_texture_create_from_vertbuf("hair_dummy_attr", g_dummy_vbo);
  }
}

void DRW_hair_update()
{
#ifndef USE_TRANSFORM_FEEDBACK
  /**
   * Workaround to transform feedback not working on mac.
   * On some system it crashes (see T58489) and on some other it renders garbage (see T60171).
   *
   * So instead of using transform feedback we render to a texture,
   * read back the result to system memory and re-upload as VBO data.
   * It is really not ideal performance wise, but it is the simplest
   * and the most local workaround that still uses the power of the GPU.
   */

  if (g_tf_calls == nullptr) {
    return;
  }

  /* Search ideal buffer size. */
  uint max_size = 0;
  for (ParticleRefineCall *pr_call = g_tf_calls; pr_call; pr_call = pr_call->next) {
    max_size = max_ii(max_size, pr_call->vert_len);
  }

  /* Create target Texture / Frame-buffer */
  /* Don't use max size as it can be really heavy and fail.
   * Do chunks of maximum 2048 * 2048 hair points. */
  int width = 2048;
  int height = min_ii(width, 1 + max_size / width);
  GPUTexture *tex = DRW_texture_pool_query_2d(
      width, height, GPU_RGBA32F, (DrawEngineType *)DRW_hair_update);
  g_tf_target_height = height;
  g_tf_target_width = width;

  GPUFrameBuffer *fb = nullptr;
  GPU_framebuffer_ensure_config(&fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(tex),
                                });

  float *data = (float *)MEM_mallocN(sizeof(float[4]) * width * height, "tf fallback buffer");

  GPU_framebuffer_bind(fb);
  while (g_tf_calls != nullptr) {
    ParticleRefineCall *pr_call = g_tf_calls;
    g_tf_calls = g_tf_calls->next;

    g_tf_id_offset = 0;
    while (pr_call->vert_len > 0) {
      int max_read_px_len = min_ii(width * height, pr_call->vert_len);

      DRW_draw_pass_subset(g_tf_pass, pr_call->shgrp, pr_call->shgrp);
      /* Read back result to main memory. */
      GPU_framebuffer_read_color(fb, 0, 0, width, height, 4, 0, GPU_DATA_FLOAT, data);
      /* Upload back to VBO. */
      GPU_vertbuf_use(pr_call->vbo);
      GPU_vertbuf_update_sub(pr_call->vbo,
                             sizeof(float[4]) * g_tf_id_offset,
                             sizeof(float[4]) * max_read_px_len,
                             data);

      g_tf_id_offset += max_read_px_len;
      pr_call->vert_len -= max_read_px_len;
    }

    MEM_freeN(pr_call);
  }

  MEM_freeN(data);
  GPU_framebuffer_free(fb);
#else
  /* Just render the pass when using compute shaders or transform feedback. */
  DRW_draw_pass(g_tf_pass);
  if (drw_hair_shader_type_get() == PART_REFINE_SHADER_COMPUTE) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  }
#endif
}

void DRW_hair_free(void)
{
  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
  DRW_TEXTURE_FREE_SAFE(g_dummy_texture);
}
