/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Particle API for render engines
 */

#include "DRW_render.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "ED_particle.h"

#include "GPU_batch.h"
#include "GPU_material.h"

#include "DEG_depsgraph_query.h"

#include "draw_cache_impl.h" /* own include */
#include "draw_hair_private.h"

static void particle_batch_cache_clear(ParticleSystem *psys);

/* ---------------------------------------------------------------------- */
/* Particle GPUBatch Cache */

typedef struct ParticlePointCache {
  GPUVertBuf *pos;
  GPUBatch *points;
  int elems_len;
  int point_len;
} ParticlePointCache;

typedef struct ParticleBatchCache {
  /* Object mode strands for hair and points for particle,
   * strands for paths when in edit mode.
   */
  ParticlePointCache point; /* Used for particle points. */

  /* Control points when in edit mode. */
  ParticleHairCache edit_hair;

  GPUVertBuf *edit_pos;
  GPUBatch *edit_strands;

  GPUVertBuf *edit_inner_pos;
  GPUBatch *edit_inner_points;
  int edit_inner_point_len;

  GPUVertBuf *edit_tip_pos;
  GPUBatch *edit_tip_points;
  int edit_tip_point_len;

  /* Settings to determine if cache is invalid. */
  bool is_dirty;
  bool edit_is_weight;
} ParticleBatchCache;

/* GPUBatch cache management. */

typedef struct HairAttributeID {
  uint pos;
  uint tan;
  uint ind;
} HairAttributeID;

typedef struct EditStrandData {
  float pos[3];
  float color;
} EditStrandData;

static GPUVertFormat *edit_points_vert_format_get(uint *r_pos_id, uint *r_color_id)
{
  static GPUVertFormat edit_point_format = {0};
  static uint pos_id, color_id;
  if (edit_point_format.attr_len == 0) {
    /* Keep in sync with EditStrandData */
    pos_id = GPU_vertformat_attr_add(&edit_point_format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    color_id = GPU_vertformat_attr_add(
        &edit_point_format, "color", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  *r_pos_id = pos_id;
  *r_color_id = color_id;
  return &edit_point_format;
}

static bool particle_batch_cache_valid(ParticleSystem *psys)
{
  ParticleBatchCache *cache = psys->batch_cache;

  if (cache == NULL) {
    return false;
  }

  if (cache->is_dirty == false) {
    return true;
  }

  return false;

  return true;
}

static void particle_batch_cache_init(ParticleSystem *psys)
{
  ParticleBatchCache *cache = psys->batch_cache;

  if (!cache) {
    cache = psys->batch_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = false;
}

static ParticleBatchCache *particle_batch_cache_get(ParticleSystem *psys)
{
  if (!particle_batch_cache_valid(psys)) {
    particle_batch_cache_clear(psys);
    particle_batch_cache_init(psys);
  }
  return psys->batch_cache;
}

void DRW_particle_batch_cache_dirty_tag(ParticleSystem *psys, int mode)
{
  ParticleBatchCache *cache = psys->batch_cache;
  if (cache == NULL) {
    return;
  }
  switch (mode) {
    case BKE_PARTICLE_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

static void particle_batch_cache_clear_point(ParticlePointCache *point_cache)
{
  GPU_BATCH_DISCARD_SAFE(point_cache->points);
  GPU_VERTBUF_DISCARD_SAFE(point_cache->pos);
}

static void particle_batch_cache_clear_hair(ParticleHairCache *hair_cache)
{
  /* TODO: more granular update tagging. */
  GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_point_buf);
  DRW_TEXTURE_FREE_SAFE(hair_cache->point_tex);

  GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_strand_buf);
  GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_strand_seg_buf);
  DRW_TEXTURE_FREE_SAFE(hair_cache->strand_tex);
  DRW_TEXTURE_FREE_SAFE(hair_cache->strand_seg_tex);

  for (int i = 0; i < MAX_MTFACE; i++) {
    GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_uv_buf[i]);
    DRW_TEXTURE_FREE_SAFE(hair_cache->uv_tex[i]);
  }
  for (int i = 0; i < MAX_MCOL; i++) {
    GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_col_buf[i]);
    DRW_TEXTURE_FREE_SAFE(hair_cache->col_tex[i]);
  }

  /* "Normal" legacy hairs */
  GPU_BATCH_DISCARD_SAFE(hair_cache->hairs);
  GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
  GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);
}

static void particle_batch_cache_clear(ParticleSystem *psys)
{
  ParticleBatchCache *cache = psys->batch_cache;
  if (!cache) {
    return;
  }

  particle_batch_cache_clear_point(&cache->point);

  particle_batch_cache_clear_hair(&cache->edit_hair);

  GPU_BATCH_DISCARD_SAFE(cache->edit_inner_points);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_inner_pos);
  GPU_BATCH_DISCARD_SAFE(cache->edit_tip_points);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_tip_pos);
}

void DRW_particle_batch_cache_free(ParticleSystem *psys)
{
  particle_batch_cache_clear(psys);
  MEM_SAFE_FREE(psys->batch_cache);
}

static void count_cache_segment_keys(ParticleCacheKey **pathcache,
                                     const int num_path_cache_keys,
                                     ParticleHairCache *hair_cache)
{
  for (int i = 0; i < num_path_cache_keys; i++) {
    ParticleCacheKey *path = pathcache[i];
    if (path->segments > 0) {
      hair_cache->strands_len++;
      hair_cache->elems_len += path->segments + 2;
      hair_cache->point_len += path->segments + 1;
    }
  }
}

static void ensure_seg_pt_count(PTCacheEdit *edit,
                                ParticleSystem *psys,
                                ParticleHairCache *hair_cache)
{
  if ((hair_cache->pos != NULL && hair_cache->indices != NULL) ||
      (hair_cache->proc_point_buf != NULL)) {
    return;
  }

  hair_cache->strands_len = 0;
  hair_cache->elems_len = 0;
  hair_cache->point_len = 0;

  if (edit != NULL && edit->pathcache != NULL) {
    count_cache_segment_keys(edit->pathcache, edit->totcached, hair_cache);
  }
  else {
    if (psys->pathcache && (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT))) {
      count_cache_segment_keys(psys->pathcache, psys->totpart, hair_cache);
    }
    if (psys->childcache) {
      const int child_count = psys->totchild * psys->part->disp / 100;
      count_cache_segment_keys(psys->childcache, child_count, hair_cache);
    }
  }
}

static float particle_key_weight(const ParticleData *particle, int strand, float t)
{
  const ParticleData *part = particle + strand;
  const HairKey *hkeys = part->hair;
  float edit_key_seg_t = 1.0f / (part->totkey - 1);
  if (t == 1.0) {
    return hkeys[part->totkey - 1].weight;
  }

  float interp = t / edit_key_seg_t;
  int index = (int)interp;
  interp -= floorf(interp); /* Time between 2 edit key */
  float s1 = hkeys[index].weight;
  float s2 = hkeys[index + 1].weight;
  return s1 + interp * (s2 - s1);
}

static int particle_batch_cache_fill_segments_edit(
    const PTCacheEdit *UNUSED(edit), /* NULL for weight data */
    const ParticleData *particle,    /* NULL for select data */
    ParticleCacheKey **path_cache,
    const int start_index,
    const int num_path_keys,
    GPUIndexBufBuilder *elb,
    GPUVertBufRaw *attr_step)
{
  int curr_point = start_index;
  for (int i = 0; i < num_path_keys; i++) {
    ParticleCacheKey *path = path_cache[i];
    if (path->segments <= 0) {
      continue;
    }
    for (int j = 0; j <= path->segments; j++) {
      EditStrandData *seg_data = (EditStrandData *)GPU_vertbuf_raw_step(attr_step);
      copy_v3_v3(seg_data->pos, path[j].co);
      float strand_t = (float)(j) / path->segments;
      if (particle) {
        float weight = particle_key_weight(particle, i, strand_t);
        /* NaN or unclamped become 1.0f */
        seg_data->color = (weight < 1.0f) ? weight : 1.0f;
      }
      else {
        /* Computed in psys_cache_edit_paths_iter(). */
        seg_data->color = path[j].col[0];
      }
      GPU_indexbuf_add_generic_vert(elb, curr_point);
      curr_point++;
    }
    /* Finish the segment and add restart primitive. */
    GPU_indexbuf_add_primitive_restart(elb);
  }
  return curr_point;
}

static void particle_batch_cache_ensure_pos(Object *object,
                                            ParticleSystem *psys,
                                            ParticlePointCache *point_cache)
{
  if (point_cache->pos != NULL) {
    return;
  }

  static GPUVertFormat format = {0};
  static uint pos_id, rot_id, val_id;
  int i, curr_point;
  ParticleData *pa;
  ParticleKey state;
  ParticleSimulationData sim = {NULL};
  const DRWContextState *draw_ctx = DRW_context_state_get();

  sim.depsgraph = draw_ctx->depsgraph;
  sim.scene = draw_ctx->scene;
  sim.ob = object;
  sim.psys = psys;
  sim.psmd = psys_get_modifier(object, psys);
  sim.psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  GPU_VERTBUF_DISCARD_SAFE(point_cache->pos);

  if (format.attr_len == 0) {
    /* initialize vertex format */
    pos_id = GPU_vertformat_attr_add(&format, "part_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    val_id = GPU_vertformat_attr_add(&format, "part_val", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    rot_id = GPU_vertformat_attr_add(&format, "part_rot", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  point_cache->pos = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(point_cache->pos, psys->totpart);

  for (curr_point = 0, i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
    state.time = DEG_get_ctime(draw_ctx->depsgraph);
    if (!psys_get_particle_state(&sim, i, &state, 0)) {
      continue;
    }

    float val;

    GPU_vertbuf_attr_set(point_cache->pos, pos_id, curr_point, state.co);
    GPU_vertbuf_attr_set(point_cache->pos, rot_id, curr_point, state.rot);

    switch (psys->part->draw_col) {
      case PART_DRAW_COL_VEL:
        val = len_v3(state.vel) / psys->part->color_vec_max;
        break;
      case PART_DRAW_COL_ACC:
        val = len_v3v3(state.vel, pa->prev_state.vel) /
              ((state.time - pa->prev_state.time) * psys->part->color_vec_max);
        break;
      default:
        val = -1.0f;
        break;
    }

    GPU_vertbuf_attr_set(point_cache->pos, val_id, curr_point, &val);

    curr_point++;
  }

  if (curr_point != psys->totpart) {
    GPU_vertbuf_data_resize(point_cache->pos, curr_point);
  }
}

static void drw_particle_update_ptcache_edit(Object *object_eval,
                                             ParticleSystem *psys,
                                             PTCacheEdit *edit)
{
  if (edit->psys == NULL) {
    return;
  }
  /* NOTE: Get flag from particle system coming from drawing object.
   * this is where depsgraph will be setting flags to.
   */
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene_orig = (Scene *)DEG_get_original_id(&draw_ctx->scene->id);
  Object *object_orig = DEG_get_original_object(object_eval);
  if (psys->flag & PSYS_HAIR_UPDATED) {
    PE_update_object(draw_ctx->depsgraph, scene_orig, object_orig, 0);
    psys->flag &= ~PSYS_HAIR_UPDATED;
  }
  if (edit->pathcache == NULL) {
    Depsgraph *depsgraph = draw_ctx->depsgraph;
    psys_cache_edit_paths(depsgraph,
                          scene_orig,
                          object_orig,
                          edit,
                          DEG_get_ctime(depsgraph),
                          DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  }
}

typedef struct ParticleDrawSource {
  Object *object;
  ParticleSystem *psys;
  ModifierData *md;
  PTCacheEdit *edit;
} ParticleDrawSource;

GPUBatch *DRW_particles_batch_cache_get_dots(Object *object, ParticleSystem *psys)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);

  if (cache->point.points == NULL) {
    particle_batch_cache_ensure_pos(object, psys, &cache->point);
    cache->point.points = GPU_batch_create(GPU_PRIM_POINTS, cache->point.pos, NULL);
  }

  return cache->point.points;
}

static void particle_batch_cache_ensure_edit_pos_and_seg(PTCacheEdit *edit,
                                                         ParticleSystem *psys,
                                                         ModifierData *UNUSED(md),
                                                         ParticleHairCache *hair_cache,
                                                         bool use_weight)
{
  if (hair_cache->pos != NULL && hair_cache->indices != NULL) {
    return;
  }

  ParticleData *particle = (use_weight) ? psys->particles : NULL;

  GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
  GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);

  GPUVertBufRaw data_step;
  GPUIndexBufBuilder elb;
  uint pos_id, color_id;
  GPUVertFormat *edit_point_format = edit_points_vert_format_get(&pos_id, &color_id);

  hair_cache->pos = GPU_vertbuf_create_with_format(edit_point_format);
  GPU_vertbuf_data_alloc(hair_cache->pos, hair_cache->point_len);
  GPU_vertbuf_attr_get_raw_data(hair_cache->pos, pos_id, &data_step);

  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, hair_cache->elems_len, hair_cache->point_len);

  if (edit != NULL && edit->pathcache != NULL) {
    particle_batch_cache_fill_segments_edit(
        edit, particle, edit->pathcache, 0, edit->totcached, &elb, &data_step);
  }
  else {
    BLI_assert_msg(0, "Hairs are not in edit mode!");
  }
  hair_cache->indices = GPU_indexbuf_build(&elb);
}

GPUBatch *DRW_particles_batch_cache_get_edit_strands(Object *object,
                                                     ParticleSystem *psys,
                                                     PTCacheEdit *edit,
                                                     bool use_weight)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->edit_is_weight != use_weight) {
    GPU_VERTBUF_DISCARD_SAFE(cache->edit_hair.pos);
    GPU_BATCH_DISCARD_SAFE(cache->edit_hair.hairs);
  }
  if (cache->edit_hair.hairs != NULL) {
    return cache->edit_hair.hairs;
  }
  drw_particle_update_ptcache_edit(object, psys, edit);
  ensure_seg_pt_count(edit, psys, &cache->edit_hair);
  particle_batch_cache_ensure_edit_pos_and_seg(edit, psys, NULL, &cache->edit_hair, use_weight);
  cache->edit_hair.hairs = GPU_batch_create(
      GPU_PRIM_LINE_STRIP, cache->edit_hair.pos, cache->edit_hair.indices);
  cache->edit_is_weight = use_weight;
  return cache->edit_hair.hairs;
}

static void ensure_edit_inner_points_count(const PTCacheEdit *edit, ParticleBatchCache *cache)
{
  if (cache->edit_inner_pos != NULL) {
    return;
  }
  cache->edit_inner_point_len = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    BLI_assert(point->totkey >= 1);
    cache->edit_inner_point_len += (point->totkey - 1);
  }
}

static void particle_batch_cache_ensure_edit_inner_pos(PTCacheEdit *edit,
                                                       ParticleBatchCache *cache)
{
  if (cache->edit_inner_pos != NULL) {
    return;
  }

  uint pos_id, color_id;
  GPUVertFormat *edit_point_format = edit_points_vert_format_get(&pos_id, &color_id);

  cache->edit_inner_pos = GPU_vertbuf_create_with_format(edit_point_format);
  GPU_vertbuf_data_alloc(cache->edit_inner_pos, cache->edit_inner_point_len);

  int global_key_index = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    for (int key_index = 0; key_index < point->totkey - 1; key_index++) {
      PTCacheEditKey *key = &point->keys[key_index];
      float color = (key->flag & PEK_SELECT) ? 1.0f : 0.0f;
      GPU_vertbuf_attr_set(cache->edit_inner_pos, pos_id, global_key_index, key->world_co);
      GPU_vertbuf_attr_set(cache->edit_inner_pos, color_id, global_key_index, &color);
      global_key_index++;
    }
  }
}

GPUBatch *DRW_particles_batch_cache_get_edit_inner_points(Object *object,
                                                          ParticleSystem *psys,
                                                          PTCacheEdit *edit)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->edit_inner_points != NULL) {
    return cache->edit_inner_points;
  }
  drw_particle_update_ptcache_edit(object, psys, edit);
  ensure_edit_inner_points_count(edit, cache);
  particle_batch_cache_ensure_edit_inner_pos(edit, cache);
  cache->edit_inner_points = GPU_batch_create(GPU_PRIM_POINTS, cache->edit_inner_pos, NULL);
  return cache->edit_inner_points;
}

static void ensure_edit_tip_points_count(const PTCacheEdit *edit, ParticleBatchCache *cache)
{
  if (cache->edit_tip_pos != NULL) {
    return;
  }
  cache->edit_tip_point_len = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    cache->edit_tip_point_len += 1;
  }
}

static void particle_batch_cache_ensure_edit_tip_pos(PTCacheEdit *edit, ParticleBatchCache *cache)
{
  if (cache->edit_tip_pos != NULL) {
    return;
  }

  uint pos_id, color_id;
  GPUVertFormat *edit_point_format = edit_points_vert_format_get(&pos_id, &color_id);

  cache->edit_tip_pos = GPU_vertbuf_create_with_format(edit_point_format);
  GPU_vertbuf_data_alloc(cache->edit_tip_pos, cache->edit_tip_point_len);

  int global_point_index = 0;
  for (int point_index = 0; point_index < edit->totpoint; point_index++) {
    const PTCacheEditPoint *point = &edit->points[point_index];
    if (point->flag & PEP_HIDE) {
      continue;
    }
    PTCacheEditKey *key = &point->keys[point->totkey - 1];
    float color = (key->flag & PEK_SELECT) ? 1.0f : 0.0f;

    GPU_vertbuf_attr_set(cache->edit_tip_pos, pos_id, global_point_index, key->world_co);
    GPU_vertbuf_attr_set(cache->edit_tip_pos, color_id, global_point_index, &color);
    global_point_index++;
  }
}

GPUBatch *DRW_particles_batch_cache_get_edit_tip_points(Object *object,
                                                        ParticleSystem *psys,
                                                        PTCacheEdit *edit)
{
  ParticleBatchCache *cache = particle_batch_cache_get(psys);
  if (cache->edit_tip_points != NULL) {
    return cache->edit_tip_points;
  }
  drw_particle_update_ptcache_edit(object, psys, edit);
  ensure_edit_tip_points_count(edit, cache);
  particle_batch_cache_ensure_edit_tip_pos(edit, cache);
  cache->edit_tip_points = GPU_batch_create(GPU_PRIM_POINTS, cache->edit_tip_pos, NULL);
  return cache->edit_tip_points;
}
